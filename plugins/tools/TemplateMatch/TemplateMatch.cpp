/**
 * @file TemplateMatch.cpp
 * @brief 模板匹配工具 — 灰度匹配 (#9 角度/缩放/多目标升级)
 *
 * 升级点:
 *   - 角度搜索: angleStart~angleEnd 按angleStep枚举旋转模板 (默认0~0=不搜角度, 兼容旧工程)
 *   - 缩放搜索: scaleMin~scaleMax 枚举缩放 (默认1~1=不搜缩放)
 *   - 多目标: maxMatches>1 时从相关系数图连续取峰并邻域抑制
 *   - 输出: matchX/Y(最佳匹配中心), angle/scale(最佳姿态), matchXs等列表(多目标)
 */
#include "TemplateMatch.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#include <opencv2/imgcodecs.hpp>
#endif
#include <algorithm>
#include <cmath>
#include <QVariantMap>
#include <QVariantList>

namespace VisionInspector {

PropertyDefList TemplateMatch::propertyDefs() const {
    return {
        PropertyDef::stringProp("templatePath", "模板图像路径", ""),
        PropertyDef::enumProp("method", "匹配方法", {"平方差", "归一化平方差", "相关系数", "归一化相关系数"}, 3),
        PropertyDef::doubleProp("threshold", "匹配阈值", 0.8, 0.0, 1.0),
        PropertyDef::doubleProp("angleStart", "角度起始", 0, -180, 180, "搜索"),
        PropertyDef::doubleProp("angleEnd", "角度结束", 0, -180, 180, "搜索"),
        PropertyDef::doubleProp("angleStep", "角度步长", 5, 0.1, 90, "搜索"),
        PropertyDef::doubleProp("scaleMin", "缩放下限", 1.0, 0.1, 5.0, "搜索"),
        PropertyDef::doubleProp("scaleMax", "缩放上限", 1.0, 0.1, 5.0, "搜索"),
        PropertyDef::intProp("maxMatches", "最大目标数", 1, 1, 100, "搜索"),
    };
}

bool TemplateMatch::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    QString templatePath = propertyValue("templatePath").toString();
    cv::Mat templ = cachedTemplateImage(templatePath, cv::IMREAD_GRAYSCALE);   // #8 模板缓存
    if (templ.empty()) { setStatus(ToolStatus::NG); setResultData("error", "无法加载模板"); return false; }

    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    const int method = propertyValue("method").toInt();
    const double threshold = propertyValue("threshold").toDouble();
    const double angleStart = propertyValue("angleStart").toDouble();
    const double angleEnd = propertyValue("angleEnd").toDouble();
    const double angleStep = std::max(0.1, propertyValue("angleStep").toDouble());
    double scaleMin = propertyValue("scaleMin").toDouble();
    double scaleMax = propertyValue("scaleMax").toDouble();
    if (scaleMin > scaleMax) std::swap(scaleMin, scaleMax);
    const int maxMatches = std::max(1, propertyValue("maxMatches").toInt());

    // 姿态枚举 (角度环回: -15~15 时按 -15..0..15 顺序; 直接线性即可)
    std::vector<double> angles;
    for (double a = angleStart; a <= angleEnd + 1e-9; a += angleStep) angles.push_back(a);
    if (angles.empty()) angles.push_back(0);
    std::vector<double> scales;
    {
        const int nSteps = std::max(1, (int)std::ceil((scaleMax - scaleMin) / 0.05));  // 缩放步长≤0.05
        for (int i = 0; i < nSteps; ++i)
            scales.push_back(scaleMin + (scaleMax - scaleMin) * i / std::max(1, nSteps - 1));
    }

    m_matches.clear();
    try {
        for (double scale : scales) {
            cv::Mat scT;
            if (std::fabs(scale - 1.0) < 1e-9) scT = templ;
            else cv::resize(templ, scT, cv::Size(), scale, scale, cv::INTER_LINEAR);
            const int stw = scT.cols, sth = scT.rows;
            const int sDia = (int)std::ceil(std::hypot(stw, sth));

            for (double angle : angles) {
                cv::Mat rotated;
                if (std::fabs(angle) < 1e-9) {
                    // 无旋转: 直接把模板放入居中画布? 不需要 — 只有单姿态且需多目标时用原图
                    if (angles.size() == 1 && scales.size() == 1) rotated = scT;
                    else {
                        rotated = cv::Mat::zeros(sDia, sDia, scT.type());
                        scT.copyTo(rotated(cv::Rect((sDia - stw) / 2, (sDia - sth) / 2, stw, sth)));
                    }
                } else {
                    const cv::Mat rot = cv::getRotationMatrix2D(
                        cv::Point2f(stw / 2.0f, sth / 2.0f), angle, 1.0);
                    cv::warpAffine(scT, rotated, rot, cv::Size(sDia, sDia),
                                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));
                }
                if (rotated.cols > src.cols || rotated.rows > src.rows) continue;

                cv::Mat result;
                cv::matchTemplate(src, rotated, result, method);

                // 多目标: 连续取峰 + 邻域抑制 (单目标时循环体只跑一次)
                cv::Mat work = result.clone();
                for (int n = 0; n < maxMatches; ++n) {
                    double minVal, maxVal; cv::Point minLoc, maxLoc;
                    cv::minMaxLoc(work, &minVal, &maxVal, &minLoc, &maxLoc);
                    const double score = (method <= 1) ? (1.0 - minVal) : maxVal;
                    const cv::Point loc = (method <= 1) ? minLoc : maxLoc;
                    if (score < threshold) break;

                    // 匹配点(画布左上) → 图像坐标模板中心
                    // 画布以模板为中心 → 画布中心即模板中心
                    const cv::Point2d center(loc.x + sDia / 2.0, loc.y + sDia / 2.0);
                    m_matches.push_back({center, score, angle, scale});

                    // 邻域抑制: 清零该峰周围一个模板画布区域
                    const int sx = std::max(0, loc.x - sDia / 2);
                    const int sy = std::max(0, loc.y - sDia / 2);
                    const int ex = std::min(work.cols, loc.x + sDia / 2);
                    const int ey = std::min(work.rows, loc.y + sDia / 2);
                    if (ex > sx && ey > sy)
                        work(cv::Rect(sx, sy, ex - sx, ey - sy)).setTo((method <= 1) ? 1.0 : -1.0);
                    if (method <= 1) { /* SQDIFF已置1.0(最差), 循环自然终止 */ }
                }
            }
        }
    } catch (const cv::Exception& e) {
        // 模板大于原图等异常不再崩溃
        setStatus(ToolStatus::NG);
        setResultData("error", QString("模板匹配异常: %1").arg(e.what()));
        return false;
    }

    std::sort(m_matches.begin(), m_matches.end(),
              [](const Match& a, const Match& b) { return a.score > b.score; });
    // 跨姿态去重: 同一位置保留最高分姿态
    std::vector<Match> uniq;
    for (const auto& m : m_matches) {
        bool dup = false;
        for (const auto& u : uniq) {
            if (std::fabs(u.center.x - m.center.x) < templ.cols / 2.0 &&
                std::fabs(u.center.y - m.center.y) < templ.rows / 2.0) { dup = true; break; }
        }
        if (!dup) uniq.push_back(m);
    }
    m_matches = uniq;

    bool found = !m_matches.empty();
    setResultData("matchCount", (int)m_matches.size());
    if (found) {
        const Match& best = m_matches.front();
        setResultData("score", best.score);
        setResultData("matchX", best.center.x);
        setResultData("matchY", best.center.y);
        setResultData("angle", best.angle);
        setResultData("scale", best.scale);
        setResultData("found", true);
        if (m_matches.size() > 1) {
            QVariantList xs, ys, scores, anglesOut, scalesOut;
            for (const auto& m : m_matches) {
                xs.append(m.center.x); ys.append(m.center.y);
                scores.append(m.score); anglesOut.append(m.angle); scalesOut.append(m.scale);
            }
            setResultData("matchXs", xs);
            setResultData("matchYs", ys);
            setResultData("scores", scores);
            setResultData("angles", anglesOut);
            setResultData("scales", scalesOut);
        }
    } else {
        setResultData("score", 0.0);
        setResultData("found", false);
    }
    setStatus(found ? ToolStatus::OK : ToolStatus::NG);
    return found;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

std::vector<QVariant> TemplateMatch::overlays() const {
    std::vector<QVariant> out;
#ifdef VI_HAS_OPENCV
    for (const auto& m : m_matches) {
        QVariantMap cross;
        cross["type"] = "cross";
        cross["cx"] = m.center.x;
        cross["cy"] = m.center.y;
        cross["size"] = 8.0;
        cross["color"] = "#00ff00";
        out.push_back(cross);
    }
#endif
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(TemplateMatch, "模板匹配", VisionInspector::ToolCategory::Detection)
