#include "CircleDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif
#include <cmath>

namespace VisionInspector {

PropertyDefList CircleDetection::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "圆形", "环形"}, 1),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiRadius", "ROI半径", 50, 1, 5000),
        PropertyDef::doubleProp("roiThickness", "ROI厚度", 20, 1, 500),
        PropertyDef::doubleProp("roiStartAngle", "ROI起始角度", 0, 0, 360),
        PropertyDef::doubleProp("roiSweepAngle", "ROI扫描角度", 360, 1, 360),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 40, 1, 255),
        PropertyDef::intProp("scanCount", "扫描数量", 5, 1, 36),
        PropertyDef::doubleProp("tolerance", "容忍误差", 1.0, 0.1, 100),
    };
}

bool CircleDetection::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.radius = propertyValue("roiRadius").toDouble();
    m_roi.thickness = propertyValue("roiThickness").toDouble();
    m_roi.startAngle = propertyValue("roiStartAngle").toDouble();
    m_roi.sweepAngle = propertyValue("roiSweepAngle").toDouble();
    int polarity = propertyValue("edgePolarity").toInt();
    int threshold = propertyValue("gradientThreshold").toInt();
    int scanCount = propertyValue("scanCount").toInt();
    double tolerance = propertyValue("tolerance").toDouble();
    double cx = m_roi.centerX, cy = m_roi.centerY;
    double radius = m_roi.radius;
    // 沿圆周扫描检测边缘点
    std::vector<cv::Point2f> edgePoints;
    for (int s = 0; s < scanCount; ++s) {
        double angle = m_roi.startAngle + (m_roi.sweepAngle * s / std::max(1, scanCount - 1));
        double rad = angle * M_PI / 180.0;
        double dx = std::cos(rad), dy = std::sin(rad);
        double sx = cx + dx * (radius - m_roi.thickness / 2);
        double sy = cy + dy * (radius - m_roi.thickness / 2);
        double ex = cx + dx * (radius + m_roi.thickness / 2);
        double ey = cy + dy * (radius + m_roi.thickness / 2);
        int steps = (int)m_roi.thickness;
        std::vector<double> profile;
        for (int i = 0; i <= steps; ++i) {
            double t = (steps > 0) ? (double)i / steps : 0;
            int px = (int)(sx + (ex - sx) * t);
            int py = (int)(sy + (ey - sy) * t);
            if (px >= 0 && px < src.cols && py >= 0 && py < src.rows)
                profile.push_back(src.at<uchar>(py, px));
            else profile.push_back(0);
        }
        double maxGrad = 0; int edgeIdx = -1;
        for (size_t i = 1; i < profile.size() - 1; ++i) {
            double grad = (profile[i+1] - profile[i-1]) / 2.0;
            bool match = (polarity == 0) ? (std::abs(grad) >= threshold) :
                         (polarity == 1) ? (grad <= -threshold) : (grad >= threshold);
            if (match && std::abs(grad) > maxGrad) { maxGrad = std::abs(grad); edgeIdx = (int)i; }
        }
        if (edgeIdx >= 0) {
            double t = (double)edgeIdx / std::max(1, steps);
            edgePoints.push_back(cv::Point2f((float)(sx + (ex - sx) * t), (float)(sy + (ey - sy) * t)));
        }
    }
    if (edgePoints.size() < 3) { setResultData("undetectedCount", scanCount - (int)edgePoints.size()); setStatus(ToolStatus::NG); return false; }
    // 最小二乘拟合圆
    double sumX = 0, sumY = 0, sumX2 = 0, sumY2 = 0, sumXY = 0, sumX3 = 0, sumY3 = 0, sumX2Y = 0, sumXY2 = 0;
    int n = (int)edgePoints.size();
    for (const auto& p : edgePoints) {
        double x = p.x, y = p.y;
        sumX += x; sumY += y; sumX2 += x*x; sumY2 += y*y; sumXY += x*y;
        sumX3 += x*x*x; sumY3 += y*y*y; sumX2Y += x*x*y; sumXY2 += x*y*y;
    }
    double A = n * sumX2 - sumX * sumX;
    double B = n * sumXY - sumX * sumY;
    double C = n * sumY2 - sumY * sumY;
    double D = 0.5 * (n * sumX3 + n * sumXY2 - sumX * sumX2 - sumX * sumY2);
    double E = 0.5 * (n * sumX2Y + n * sumY3 - sumY * sumX2 - sumY * sumY2);
    double denom = A * C - B * B;
    if (std::abs(denom) < 1e-10) { setStatus(ToolStatus::NG); return false; }
    double fitCx = (D * C - B * E) / denom;
    double fitCy = (A * E - B * D) / denom;
    double fitRadius = 0;
    for (const auto& p : edgePoints) {
        fitRadius += std::sqrt((p.x - fitCx) * (p.x - fitCx) + (p.y - fitCy) * (p.y - fitCy));
    }
    fitRadius /= n;
    setResultData("centerX", fitCx);
    setResultData("centerY", fitCy);
    setResultData("radius", fitRadius);
    setResultData("anomalyCount", 0);
    setResultData("undetectedCount", scanCount - (int)edgePoints.size());
    setResultData("edgeCount", (int)edgePoints.size());
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CircleDetection, "检测圆形", VisionInspector::ToolCategory::Detection)
