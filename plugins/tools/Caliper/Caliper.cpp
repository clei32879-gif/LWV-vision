/**
 * @file Caliper.cpp
 * @brief 卡尺测量工具 — 亚像素版
 *
 * 算法 (阶段2升级):
 *   1. 旋转卡尺: 沿ROI角度方向的扫描线(长度=roiWidth, 宽度=scanWidth平均)
 *   2. 双线性剖面采样 + 梯度峰值抛物线内插 → 亚像素边缘位置
 * 输出: edgePositionX/Y(图像坐标), width(沿扫描方向距离), strength, found
 */
#include "Caliper.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <cmath>
#include <QVariantMap>

namespace VisionInspector {

PropertyDefList Caliper::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 1),
        PropertyDef::doubleProp("roiCenterX", "卡尺中心X", 320, 0, 10000, "卡尺"),
        PropertyDef::doubleProp("roiCenterY", "卡尺中心Y", 240, 0, 10000, "卡尺"),
        PropertyDef::doubleProp("roiWidth", "卡尺长度(扫描距离)", 120, 1, 10000, "卡尺"),
        PropertyDef::doubleProp("roiHeight", "卡尺宽度(平均高)", 10, 1, 1000, "卡尺"),
        PropertyDef::doubleProp("roiAngle", "卡尺角度", 0, -180, 180, "卡尺"),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "卡尺"),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0, "检测"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 30, 1, 255, "检测"),
        PropertyDef::intProp("filterHalfWidth", "梯度平滑半宽", 2, 1, 20, "检测"),
    };
}

bool Caliper::execute(ToolContext& context) {
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

    double cx = propertyValue("roiCenterX").toDouble();
    double cy = propertyValue("roiCenterY").toDouble();
    const double roiW = propertyValue("roiWidth").toDouble();
    const double roiH = propertyValue("roiHeight").toDouble();
    double roiAng = propertyValue("roiAngle").toDouble();
    applyCorrection(context, cx, cy, roiAng);   // 位置补正跟随

    ScanOptions opt;
    opt.polarity = propertyValue("edgePolarity").toInt();
    opt.gradThreshold = propertyValue("gradientThreshold").toInt();
    opt.filterHalfWidth = propertyValue("filterHalfWidth").toInt();

    // 扫描线: 沿卡尺角度方向, 从起点到终点
    const double rad = roiAng * CV_PI / 180.0;
    const cv::Point2d u(std::cos(rad), std::sin(rad));
    m_scanStart = cv::Point2d(cx - u.x * roiW / 2, cy - u.y * roiW / 2);
    m_scanEnd   = cv::Point2d(cx + u.x * roiW / 2, cy + u.y * roiW / 2);

    SubpixEdgePoint ep;
    m_lastOk = findEdgeSubpix(src, m_scanStart, m_scanEnd, opt, ep);
    if (!m_lastOk) {
        setResultData("found", false);
        setResultData("error", "扫描方向未找到超阈值边缘");
        setStatus(ToolStatus::NG);
        return false;
    }

    m_edge = ep.pos;
    const double dist = std::hypot(ep.pos.x - m_scanStart.x, ep.pos.y - m_scanStart.y);
    setResultData("edgePositionX", ep.pos.x);
    setResultData("edgePositionY", ep.pos.y);
    setResultData("width", dist);
    setResultData("strength", ep.strength);
    setResultData("found", true);
    setStatus(ToolStatus::OK);
    return true;
#endif
}

std::vector<QVariant> Caliper::overlays() const {
    std::vector<QVariant> out;
#ifdef VI_HAS_OPENCV
    // 卡尺框(青色)
    QVariantMap box;
    box["type"] = "line";
    box["x1"] = m_scanStart.x; box["y1"] = m_scanStart.y;
    box["x2"] = m_scanEnd.x;   box["y2"] = m_scanEnd.y;
    box["color"] = "#00aaff";
    out.push_back(box);
    if (m_lastOk) {
        // 找到的边缘点(黄色)
        QVariantMap cross;
        cross["type"] = "cross";
        cross["cx"] = m_edge.x;
        cross["cy"] = m_edge.y;
        cross["size"] = 6.0;
        cross["color"] = "#ffff00";
        out.push_back(cross);
    }
#endif
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(Caliper, "卡尺测量", VisionInspector::ToolCategory::Geometry)
