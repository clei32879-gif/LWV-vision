/**
 * @file LineDetection.cpp
 * @brief 检测直线工具 — 亚像素版
 *
 * 算法 (阶段2升级):
 *   1. 支持旋转矩形ROI: 沿ROI长边布置卡尺组, 每个卡尺垂直于长边方向扫描
 *   2. 双线性剖面采样 + 梯度峰值抛物线内插 → 亚像素边缘点
 *   3. 鲁棒直线拟合(质心+PCA主方向 + 中位数残差外点剔除)
 * 输出: centerX/centerY/angle/x1/y1/x2/y2/rms/edgeCount
 */
#include "LineDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <cmath>
#include <QVariantMap>

namespace VisionInspector {

PropertyDefList LineDetection::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形"}, 1),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 320, 0, 10000, "ROI"),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 240, 0, 10000, "ROI"),
        PropertyDef::doubleProp("roiWidth", "ROI宽度(长边)", 200, 1, 10000, "ROI"),
        PropertyDef::doubleProp("roiHeight", "ROI高度(扫描向)", 40, 1, 10000, "ROI"),
        PropertyDef::doubleProp("roiAngle", "ROI角度", 0, -180, 180, "ROI"),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0, "检测"),
        PropertyDef::enumProp("edgePosition", "边缘位置", {"最强", "首个", "末个"}, 0, "检测"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 30, 1, 255, "检测"),
        PropertyDef::intProp("filterHalfWidth", "梯度平滑半宽", 2, 1, 20, "检测"),
        PropertyDef::intProp("scanCount", "卡尺数量", 16, 2, 200, "检测"),
        PropertyDef::doubleProp("tolerance", "容忍误差(px)", 10.0, 0.1, 100, "检测"),
    };
}

bool LineDetection::execute(ToolContext& context) {
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
    const int scanCount = propertyValue("scanCount").toInt();
    const int edgePos = propertyValue("edgePosition").toInt();

    // ROI局部坐标系: 长边方向u=(cos,sin), 扫描方向v=(-sin,cos)
    const double rad = roiAng * CV_PI / 180.0;
    const cv::Point2d u(std::cos(rad), std::sin(rad));
    const cv::Point2d v(-std::sin(rad), std::cos(rad));

    // #2 形状ROI: 菱形/圆形时基点在形状外的扫描线跳过
    QRectF roiRectFull(cx - roiW / 2, cy - roiH / 2, roiW, roiH);
    cv::Mat shapeMaskFull;
    {
        m_roi.type = (ROIType)propertyValue("roiType").toInt();
        m_roi.centerX = cx; m_roi.centerY = cy;
        m_roi.width = roiW; m_roi.height = roiH;
        shapeMaskFull = makeRoiShapeMask(m_roi, roiRectFull);
    }

    std::vector<cv::Point2d> edgePts;
    for (int s = 0; s < scanCount; ++s) {
        const double t = (scanCount == 1) ? 0.0 : -0.5 + (double)s / (scanCount - 1);
        const cv::Point2d base(cx + u.x * (t * roiW), cy + u.y * (t * roiW));
        if (!shapeMaskFull.empty()) {
            const int mx = (int)std::lround(base.x - roiRectFull.x());
            const int my = (int)std::lround(base.y - roiRectFull.y());
            if (mx < 0 || my < 0 || mx >= shapeMaskFull.cols || my >= shapeMaskFull.rows ||
                shapeMaskFull.at<uchar>(my, mx) == 0)
                continue;
        }
        const cv::Point2d p0 = base - v * (roiH / 2);
        const cv::Point2d p1 = base + v * (roiH / 2);

        if (edgePos == 0) {           // 最强
            SubpixEdgePoint ep;
            if (findEdgeSubpix(src, p0, p1, opt, ep))
                edgePts.push_back(ep.pos);
        } else {
            auto all = findEdgesSubpix(src, p0, p1, opt, 0);
            if (!all.empty())
                edgePts.push_back(edgePos == 1 ? all.front().pos : all.back().pos);
        }
    }

    m_lastEdges = edgePts;
    m_lastOk = false;
    setResultData("edgeCount", (int)edgePts.size());

    cv::Point2d dir, origin;
    double rms = 0;
    if (!fitLineRobust(edgePts, dir, origin, rms)) {
        setStatus(ToolStatus::NG);
        setResultData("error", "有效边缘点不足");
        return false;
    }

    m_lastOk = true;
    // 输出线上两点(沿方向 ± roiW/2)
    m_p1 = origin - dir * (roiW / 2);
    m_p2 = origin + dir * (roiW / 2);
    double angle = std::atan2(dir.y, dir.x) * 180.0 / CV_PI;
    if (angle > 90) angle -= 180;
    if (angle < -90) angle += 180;

    // tolerance 死属性激活: 拟合RMS超差判定 (单位=像素)
    const double tol = propertyValue("tolerance").toDouble();
    if (tol > 0 && rms > tol) {
        setResultData("centerX", origin.x);
        setResultData("centerY", origin.y);
        setResultData("angle", angle);
        setResultData("rms", rms);
        setResultData("fitFailed", true);
        setResultData("error", QString("拟合误差%1px 超出容差%2px").arg(rms).arg(tol));
        setStatus(ToolStatus::NG);
        return false;
    }

    setResultData("centerX", origin.x);
    setResultData("centerY", origin.y);
    setResultData("angle", angle);
    setResultData("x1", m_p1.x);
    setResultData("y1", m_p1.y);
    setResultData("x2", m_p2.x);
    setResultData("y2", m_p2.y);
    setResultData("rms", rms);
    setStatus(ToolStatus::OK);
    return true;
#endif
}

std::vector<QVariant> LineDetection::overlays() const {
    std::vector<QVariant> out;
#ifdef VI_HAS_OPENCV
    if (!m_lastOk) return out;
    QVariantMap pts;
    pts["type"] = "points";
    QVariantList plist;
    for (const auto& p : m_lastEdges)
        plist.append(QVariantList{p.x, p.y});
    pts["pts"] = plist;
    pts["color"] = "#00ff00";
    out.push_back(pts);

    QVariantMap line;
    line["type"] = "line";
    line["x1"] = m_p1.x; line["y1"] = m_p1.y;
    line["x2"] = m_p2.x; line["y2"] = m_p2.y;
    line["color"] = "#00ff88";
    out.push_back(line);
#endif
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(LineDetection, "检测直线", VisionInspector::ToolCategory::Detection)
