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
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>
#endif
#include <cmath>
#include <QVariantMap>

namespace VisionInspector {

PropertyDefList EdgeCircleFind::propertyDefs() const {
    return {
        PropertyDef::doubleProp("searchCenterX", "搜索中心X", 0, -1, 10000, "搜索"),
        PropertyDef::doubleProp("searchCenterY", "搜索中心Y", 0, -1, 10000, "搜索"),
        PropertyDef::doubleProp("searchRange", "搜索半径(0=全图)", 0, 0, 5000, "搜索"),
        PropertyDef::doubleProp("minRadius", "最小半径", 20, 1, 5000, "搜索"),
        PropertyDef::doubleProp("maxRadius", "最大半径", 500, 1, 5000, "搜索"),
        PropertyDef::doubleProp("expectedRadius", "期望半径(0=任意)", 0, 0, 5000, "搜索"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 30, 8, 255, "检测"),
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
