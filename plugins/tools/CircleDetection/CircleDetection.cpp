/**
 * @file CircleDetection.cpp
 * @brief 检测圆形工具 — 亚像素版
 *
 * 算法 (阶段2升级):
 *   1. 沿ROI圆周均匀布置卡尺, 每个卡尺沿径向扫描灰度剖面(双线性采样)
 *   2. 梯度峰值 + 抛物线三点插值 → 亚像素边缘点
 *   3. 鲁棒圆拟合: Kasa代数拟合 + 中位数残差外点剔除(最多2轮)
 * 输出: centerX/centerY/radius/rms/edgeCount/undetectedCount
 */
#include "CircleDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <cmath>
#include <QVariantMap>

namespace VisionInspector {

PropertyDefList CircleDetection::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "圆形", "环形"}, 1),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 320, 0, 10000, "ROI"),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 240, 0, 10000, "ROI"),
        PropertyDef::doubleProp("roiRadius", "ROI半径", 110, 1, 5000, "ROI"),
        PropertyDef::doubleProp("roiThickness", "卡尺长度(搜索带宽)", 24, 1, 500, "ROI"),
        PropertyDef::doubleProp("roiStartAngle", "起始角度", 0, -360, 360, "ROI"),
        PropertyDef::doubleProp("roiSweepAngle", "扫描角度", 360, 1, 360, "ROI"),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0, "检测"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 30, 1, 255, "检测"),
        PropertyDef::intProp("filterHalfWidth", "梯度平滑半宽", 2, 1, 20, "检测"),
        PropertyDef::intProp("scanCount", "卡尺数量", 24, 3, 180, "检测"),
        PropertyDef::doubleProp("tolerance", "容忍误差", 1.0, 0.1, 100, "检测"),
    };
}

bool CircleDetection::execute(ToolContext& context) {
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

    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.radius = propertyValue("roiRadius").toDouble();
    m_roi.thickness = propertyValue("roiThickness").toDouble();
    m_roi.startAngle = propertyValue("roiStartAngle").toDouble();
    m_roi.sweepAngle = propertyValue("roiSweepAngle").toDouble();

    ScanOptions opt;
    opt.polarity = propertyValue("edgePolarity").toInt();
    opt.gradThreshold = propertyValue("gradientThreshold").toInt();
    opt.filterHalfWidth = propertyValue("filterHalfWidth").toInt();
    const int scanCount = propertyValue("scanCount").toInt();

    // 沿圆周布置卡尺, 径向扫描
    const double innerR = std::max(1.0, m_roi.radius - m_roi.thickness / 2);
    const double outerR = m_roi.radius + m_roi.thickness / 2;
    std::vector<cv::Point2d> edgePts;
    int undetected = 0;

    for (int s = 0; s < scanCount; ++s) {
        const double angleDeg = m_roi.startAngle +
            m_roi.sweepAngle * (scanCount == 1 ? 0.5 : (double)s / (scanCount - 1));
        const double rad = angleDeg * CV_PI / 180.0;
        const cv::Point2d p0(m_roi.centerX + std::cos(rad) * innerR,
                             m_roi.centerY + std::sin(rad) * innerR);
        const cv::Point2d p1(m_roi.centerX + std::cos(rad) * outerR,
                             m_roi.centerY + std::sin(rad) * outerR);
        SubpixEdgePoint ep;
        if (findEdgeSubpix(src, p0, p1, opt, ep))
            edgePts.push_back(ep.pos);
        else
            ++undetected;
    }

    m_lastEdges.clear();
    m_lastOk = false;
    setResultData("undetectedCount", undetected);
    setResultData("edgeCount", (int)edgePts.size());

    cv::Point2d center;
    double radius = 0, rms = 0;
    if (!fitCircleRobust(edgePts, center, radius, rms)) {
        setStatus(ToolStatus::NG);
        setResultData("error", "有效边缘点不足或分布退化");
        return false;
    }

    m_lastOk = true;
    m_lastCenter = center;
    m_lastRadius = radius;
    m_lastEdges = edgePts;

    setResultData("centerX", center.x);
    setResultData("centerY", center.y);
    setResultData("radius", radius);
    setResultData("rms", rms);
    setStatus(ToolStatus::OK);
    return true;
#endif
}

std::vector<QVariant> CircleDetection::overlays() const {
    std::vector<QVariant> out;
#ifdef VI_HAS_OPENCV
    if (!m_lastOk) return out;
    // 边缘点(绿色)
    QVariantMap pts;
    pts["type"] = "points";
    QVariantList plist;
    for (const auto& p : m_lastEdges)
        plist.append(QVariantList{p.x, p.y});
    pts["pts"] = plist;
    pts["color"] = "#00ff00";
    out.push_back(pts);
    // 拟合圆(按状态着色)
    QVariantMap circle;
    circle["type"] = "circle";
    circle["cx"] = m_lastCenter.x;
    circle["cy"] = m_lastCenter.y;
    circle["r"] = m_lastRadius;
    circle["color"] = "#00ff88";
    out.push_back(circle);
    // 十字标记圆心
    QVariantMap cross;
    cross["type"] = "cross";
    cross["cx"] = m_lastCenter.x;
    cross["cy"] = m_lastCenter.y;
    cross["size"] = 8.0;
    cross["color"] = "#ffff00";
    out.push_back(cross);
#endif
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CircleDetection, "检测圆形", VisionInspector::ToolCategory::Detection)
