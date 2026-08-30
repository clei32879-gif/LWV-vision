#include "GrayscaleMatch.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#include <opencv2/imgcodecs.hpp>

#endif
#include <QFile>

namespace VisionInspector {

PropertyDefList GrayscaleMatch::propertyDefs() const {
    return {
        PropertyDef::stringProp("templatePath", "模板图像路径", ""),
        PropertyDef::doubleProp("threshold", "匹配阈值", 0.7, 0.0, 1.0),
        PropertyDef::intProp("maxMatches", "最大匹配数", 1, 1, 100),
        PropertyDef::doubleProp("angleStart", "角度搜索起始(度)", -15.0, -180.0, 180.0),
        PropertyDef::doubleProp("angleEnd", "角度搜索结束(度)", 15.0, -180.0, 180.0),
        PropertyDef::doubleProp("angleStep", "角度步长(度)", 1.0, 0.1, 45.0),
        PropertyDef::doubleProp("scaleMin", "缩放最小值", 0.9, 0.1, 2.0),
        PropertyDef::doubleProp("scaleMax", "缩放最大值", 1.1, 0.1, 2.0),
    };
}

bool GrayscaleMatch::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        setStatus(ToolStatus::NG);
        setResultData("error", "输入图像为空");
        return false;
    }

    QString templatePath = propertyValue("templatePath").toString();
    double threshold = propertyValue("threshold").toDouble();
    int maxMatches = propertyValue("maxMatches").toInt();
    // 旋转/缩放搜索 (M-43: 让搜索参数真正生效)
    const double angleStart = propertyValue("angleStart").toDouble();
    const double angleEnd = propertyValue("angleEnd").toDouble();
    const double angleStep = std::max(0.1, propertyValue("angleStep").toDouble());
    const double scaleMin = std::max(0.1, propertyValue("scaleMin").toDouble());
    const double scaleMax = std::max(scaleMin, propertyValue("scaleMax").toDouble());

    cv::Mat templ = cv::imread(templatePath.toUtf8().constData(), cv::IMREAD_GRAYSCALE);
    if (templ.empty()) {
        setStatus(ToolStatus::NG);
        setResultData("error", QString("无法加载模板: %1").arg(templatePath));
        return false;
    }

    cv::Mat src;
    if (input->channels() > 1)
        cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else
        src = *input;

    // ---- 角度/缩放搜索: 在模板域生成多种姿态的模板, 取全局最佳匹配 ----
    struct Match { double x; double y; double score; double angle; double scale; };
    std::vector<Match> matches;

    // 对模板做旋转(绕模板中心), 保留背景填充边界
    const int tw = templ.cols, th = templ.rows;
    const double dia = std::ceil(std::hypot(tw, th));
    const cv::Size canv((int)dia, (int)dia);
    const cv::Point2f tCenter(tw / 2.0f, th / 2.0f);

    std::vector<double> angles;
    for (double a = angleStart; a <= angleEnd + 1e-9; a += angleStep) angles.push_back(a);
    std::vector<double> scales;
    const double sStep = std::max(0.02, (scaleMax - scaleMin) / 10.0);
    for (double s = scaleMin; s <= scaleMax + 1e-9; s += sStep) scales.push_back(s);

    cv::Mat result;
    for (double scale : scales) {
        cv::Mat scT;
        cv::resize(templ, scT, cv::Size(), scale, scale, cv::INTER_LINEAR);
        const int stw = scT.cols, sth = scT.rows;
        const double sDia = std::ceil(std::hypot(stw, sth));
        for (double angle : angles) {
            cv::Mat rot = cv::getRotationMatrix2D(cv::Point2f(stw / 2.0f, sth / 2.0f), angle, 1.0);
            // 平移使旋转后的模板仍位于画布中心 (原模板中不含旋转中心偏差)
            cv::Mat rotated;
            cv::warpAffine(scT, rotated, rot, cv::Size((int)sDia, (int)sDia),
                           cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));
            if (rotated.cols > src.cols || rotated.rows > src.rows) continue;
            cv::matchTemplate(src, rotated, result, cv::TM_CCOEFF_NORMED);
            double minVal, maxVal; cv::Point minLoc, maxLoc;
            cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
            if (maxVal < threshold) continue;
            // 旋转画布上的匹配点 → 换算回模板中心坐标
            const double rotC = (sDia - stw) / 2.0;      // 画布上模板左上角偏移(近似)
            const double offX = maxLoc.x + rotC + stw / 2.0;
            const double offY = maxLoc.y + rotC + sth / 2.0;
            const double rad = angle * CV_PI / 180.0;
            const double cc = std::cos(rad), ss = std::sin(rad);
            const double cx = stw / 2.0, cy = sth / 2.0;
            // 反旋转到未旋转模板坐标
            const double rx = cx + (offX - cx) * cc + (offY - cy) * ss;
            const double ry = cy - (offX - cx) * ss + (offY - cy) * cc;
            matches.push_back({rx, ry, maxVal, angle, scale});
        }
    }

    std::sort(matches.begin(), matches.end(),
              [](const Match& a, const Match& b) { return a.score > b.score; });
    // 去重: 同一位置附近的重复候选仅保留最高分
    std::vector<Match> uniq;
    for (const auto& m : matches) {
        bool dup = false;
        for (const auto& u : uniq) {
            if (std::fabs(u.x - m.x) < templ.cols / 2.0 && std::fabs(u.y - m.y) < templ.rows / 2.0) { dup = true; break; }
        }
        if (!dup) uniq.push_back(m);
        if ((int)uniq.size() >= maxMatches) break;
    }

    setResultData("matchCount", (int)uniq.size());
    if (!uniq.empty()) {
        setResultData("matchX", uniq[0].x);
        setResultData("matchY", uniq[0].y);
        setResultData("score", uniq[0].score);
        setResultData("matchAngle", uniq[0].angle);
        setResultData("matchScale", uniq[0].scale);
        setResultData("found", true);
        setStatus(ToolStatus::OK);
    } else {
        setResultData("found", false);
        setResultData("score", 0.0);
        setStatus(ToolStatus::NG);
    }
    return !uniq.empty();
#else
    setResultData("error", "需要OpenCV库");
    setStatus(ToolStatus::NG);
    return false;
#endif
}

VI_REGISTER_TOOL(GrayscaleMatch, "灰度匹配", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector
