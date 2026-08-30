/**
 * @file EdgeCircleFind.cpp
 * @brief 快速找圆工具 — EdgeDrawing(EDCircles) 整图搜索
 *
 * 与"检测圆形"(需先验ROI)不同, 本工具无需先验位置:
 *   1. EdgeDrawing 无参数边缘检测 → EDCircles 找圆/椭圆
 *   2. 在搜索中心附近、半径区间内选最优圆
 *   3. 厚边缘会检出内外两圆, 自动取均值(亚像素取中)
 * 典型用途: 流程第一个工具, 找到工件后经 位置补正 引导后续工具。
 * 输出: centerX/centerY/radius/score/candidateCount
 */
#include "EdgeCircleFind.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>
#endif
#include <cmath>
#include <algorithm>
#include <QVariantMap>

namespace VisionInspector {

namespace {
/** Hough 圆检测 + 卡尺亚像素细化 (回退定位: 应对毛刺/缺牙破坏外缘连续性) */
bool findCircleHoughFallback(const cv::Mat& gray,
                             const cv::Point2d& searchCenter, double searchRange,
                             double minR, double maxR, double expectR,
                             int gradThreshold,
                             cv::Point2d& outCenter, double& outRadius) {
    // 1) HOUGH_GRADIENT_ALT 整图投票找圆 (鲁棒于断弧/毛刺)
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    std::vector<cv::Vec3f> circles;
    const double dp = 1.5;
    const double minDist = std::max(2.0 * minR, 20.0);
    const double param1 = std::max(60.0, gradThreshold * 1.5);   // Canny 高阈值
    const double param2 = 0.45;                                  // ALT: 圆"完美度"阈值(0~1)
    cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT_ALT, dp, minDist,
                     param1, param2, minR, maxR);
    if (circles.empty()) return false;

    // 2) 按与 EdgeDrawing 相同的评分选最优: 靠近搜索中心 + 半径匹配期望
    struct Cand { cv::Point2d c; double r; double score; };
    std::vector<Cand> cands;
    for (const auto& v : circles) {
        const cv::Point2d c(v[0], v[1]);
        const double r = v[2];
        if (r < minR || r > maxR) continue;
        const double dist = std::hypot(c.x - searchCenter.x, c.y - searchCenter.y);
        if (searchRange > 0 && dist > searchRange) continue;
        double score = dist;
        if (expectR > 0) score += std::fabs(r - expectR) * 2.0;
        cands.push_back({c, r, score});
    }
    if (cands.empty()) return false;
    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.score < b.score; });
    const Cand& best = cands.front();

    // 3) 卡尺亚像素细化: 从 Hough 圆心向外发射径向卡尺, 在期望半径邻域找亚像素边,
    //    鲁棒圆拟合剔除毛刺外点 → 输出亚像素精度圆心/半径
    const int rays = 72;
    const double band = std::clamp(best.r * 0.20, 8.0, 40.0);   // 扫描带宽
    ScanOptions opt;
    opt.polarity = 0;
    opt.gradThreshold = std::max(8, gradThreshold * 3 / 4);
    opt.filterHalfWidth = 2;
    std::vector<cv::Point2d> edgePts;
    for (int i = 0; i < rays; ++i) {
        const double a = 2.0 * CV_PI * i / rays;
        const cv::Point2d dir(std::cos(a), std::sin(a));
        const cv::Point2d p0(best.c.x + dir.x * (best.r - band),
                             best.c.y + dir.y * (best.r - band));
        const cv::Point2d p1(best.c.x + dir.x * (best.r + band),
                             best.c.y + dir.y * (best.r + band));
        SubpixEdgePoint ep;
        if (findEdgeSubpix(gray, p0, p1, opt, ep))
            edgePts.push_back(ep.pos);
    }
    if ((int)edgePts.size() >= 6) {
        cv::Point2d c; double r = 0, rms = 0;
        if (fitCircleRobust(edgePts, c, r, rms) && r >= minR && r <= maxR) {
            outCenter = c;
            outRadius = r;
            return true;
        }
    }
    // 细化失败则退回 Hough 结果
    outCenter = best.c;
    outRadius = best.r;
    return true;
}
} // anonymous namespace

PropertyDefList EdgeCircleFind::propertyDefs() const {
    return {
        PropertyDef::doubleProp("searchCenterX", "搜索中心X", 0, -1, 10000, "搜索"),
        PropertyDef::doubleProp("searchCenterY", "搜索中心Y", 0, -1, 10000, "搜索"),
        PropertyDef::doubleProp("searchRange", "搜索半径(0=全图)", 0, 0, 5000, "搜索"),
        PropertyDef::doubleProp("minRadius", "最小半径", 20, 1, 5000, "搜索"),
        PropertyDef::doubleProp("maxRadius", "最大半径", 500, 1, 5000, "搜索"),
        PropertyDef::doubleProp("expectedRadius", "期望半径(0=任意)", 0, 0, 5000, "搜索"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 30, 8, 255, "检测"),
        PropertyDef::enumProp("fallbackMethod", "回退定位", {"禁用", "Hough圆检测"}, 1, "检测"),
    };
}

bool EdgeCircleFind::execute(ToolContext& context) {
#ifndef VI_HAS_OPENCV
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#else
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        setResultData("error", "输入图像为空"); setStatus(ToolStatus::NG); return false;
    }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    // 搜索中心: 属性<0表示用图像中心
    cv::Point2d searchCenter(
        propertyValue("searchCenterX").toDouble() < 0 ? src.cols / 2.0
            : propertyValue("searchCenterX").toDouble(),
        propertyValue("searchCenterY").toDouble() < 0 ? src.rows / 2.0
            : propertyValue("searchCenterY").toDouble());
    const double searchRange = propertyValue("searchRange").toDouble();
    const double minR = propertyValue("minRadius").toDouble();
    const double maxR = propertyValue("maxRadius").toDouble();
    const double expectR = propertyValue("expectedRadius").toDouble();

    m_lastOk = false;
    cv::ximgproc::EdgeDrawing::Params params;
    params.GradientThresholdValue = propertyValue("gradientThreshold").toInt();
    params.AnchorThresholdValue = std::max(4, params.GradientThresholdValue / 4);
    auto ed = cv::ximgproc::createEdgeDrawing();
    ed->params = params;
    ed->detectEdges(src);

    std::vector<cv::Vec6d> ellipses;
    ed->detectEllipses(ellipses);
    setResultData("candidateCount", (int)ellipses.size());

    // 收集所有"圆型"候选(Vec6d: [0]cx [1]cy [2]半径, [3..5]椭圆参数非0表示椭圆)
    struct Cand { cv::Point2d c; double r; double score; };
    std::vector<Cand> cands;
    for (const auto& e : ellipses) {
        if (std::fabs(e[3]) > 1e-9 || std::fabs(e[4]) > 1e-9)
            continue;   // 椭圆, 跳过
        const double r = e[2];
        if (r < minR || r > maxR) continue;
        const double dist = std::hypot(e[0] - searchCenter.x, e[1] - searchCenter.y);
        if (searchRange > 0 && dist > searchRange) continue;
        // 评分: 期望半径匹配 + 离搜索中心近
        double score = dist;
        if (expectR > 0) score += std::fabs(r - expectR) * 2.0;
        cands.push_back({cv::Point2d(e[0], e[1]), r, score});
    }
    if (cands.empty()) {
        // 回退定位: EdgeDrawing 因毛刺/缺牙破坏外缘连续性而未输出整圆弧时,
        // 用 Hough 圆检测重新定位 + 卡尺亚像素细化。
        const int fb = propertyValue("fallbackMethod").toInt();
        if (fb > 0 && findCircleHoughFallback(src, searchCenter, searchRange,
                                              minR, maxR, expectR,
                                              propertyValue("gradientThreshold").toInt(),
                                              m_lastCenter, m_lastRadius)) {
            m_lastOk = true;
            setResultData("centerX", m_lastCenter.x);
            setResultData("centerY", m_lastCenter.y);
            setResultData("radius", m_lastRadius);
            setResultData("score", 0.0);
            setResultData("method", "hough");
            setStatus(ToolStatus::OK);
            return true;
        }
        setStatus(ToolStatus::NG);
        setResultData("error", "未找到符合条件的圆");
        return false;
    }
    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.score < b.score; });

    // 厚边缘内外两圆取均值: 找圆心几乎重合(<=2px)的配对
    const Cand& best = cands[0];
    double radius = best.r;
    int merged = 0;
    for (size_t i = 1; i < cands.size(); ++i) {
        if (std::hypot(cands[i].c.x - best.c.x, cands[i].c.y - best.c.y) <= 2.0) {
            radius = (radius * (1 + merged) + cands[i].r) / (2 + merged);
            ++merged;
        }
    }

    m_lastOk = true;
    m_lastCenter = best.c;
    m_lastRadius = radius;
    setResultData("centerX", best.c.x);
    setResultData("centerY", best.c.y);
    setResultData("radius", radius);
    setResultData("score", best.score);
    setResultData("method", "ed");
    setStatus(ToolStatus::OK);
    return true;
#endif
}

std::vector<QVariant> EdgeCircleFind::overlays() const {
    std::vector<QVariant> out;
#ifdef VI_HAS_OPENCV
    if (!m_lastOk) return out;
    QVariantMap circle;
    circle["type"] = "circle";
    circle["cx"] = m_lastCenter.x;
    circle["cy"] = m_lastCenter.y;
    circle["r"] = m_lastRadius;
    circle["color"] = "#00ff88";
    out.push_back(circle);
    QVariantMap cross;
    cross["type"] = "cross";
    cross["cx"] = m_lastCenter.x;
    cross["cy"] = m_lastCenter.y;
    cross["size"] = 10.0;
    cross["color"] = "#ffff00";
    out.push_back(cross);
#endif
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(EdgeCircleFind, "快速找圆", VisionInspector::ToolCategory::Detection)
