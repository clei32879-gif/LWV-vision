#include "LineDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif
#include <cmath>

namespace VisionInspector {

PropertyDefList LineDetection::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::doubleProp("roiAngle", "ROI角度", 0, -180, 180),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0),
        PropertyDef::enumProp("edgePosition", "边缘位置", {"起始", "最近", "最远", "最强"}, 0),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 40, 1, 255),
        PropertyDef::intProp("filterHalfWidth", "滤波半宽", 1, 1, 20),
        PropertyDef::intProp("scanCount", "扫描数量", 5, 1, 100),
        PropertyDef::intProp("scanWidth", "扫描宽度", 5, 1, 100),
        PropertyDef::doubleProp("tolerance", "容忍误差", 1.0, 0.1, 100),
        PropertyDef::doubleProp("sampleThreshold", "采样阈值", 1.0, 0.1, 100),
    };
}

bool LineDetection::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.width = propertyValue("roiWidth").toDouble();
    m_roi.height = propertyValue("roiHeight").toDouble();
    m_roi.angle = propertyValue("roiAngle").toDouble();
    int polarity = propertyValue("edgePolarity").toInt();
    int threshold = propertyValue("gradientThreshold").toInt();
    int scanCount = propertyValue("scanCount").toInt();
    int scanWidth = propertyValue("scanWidth").toInt();
    double tolerance = propertyValue("tolerance").toDouble();
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); return false; }
    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    // 多扫描线检测边缘点
    std::vector<cv::Point2f> edgePoints;
    for (int s = 0; s < scanCount; ++s) {
        double yRatio = (scanCount > 1) ? (double)s / (scanCount - 1) : 0.5;
        int scanY = (int)(yRatio * rh);
        std::vector<double> profile;
        for (int x = 0; x < rw; ++x) {
            double sum = 0; int count = 0;
            for (int dy = -scanWidth/2; dy <= scanWidth/2; ++dy) {
                int y = scanY + dy;
                if (y >= 0 && y < rh) { sum += roiImg.at<uchar>(y, x); count++; }
            }
            profile.push_back(count > 0 ? sum / count : 0);
        }
        std::vector<double> gradient(profile.size());
        for (size_t i = 1; i < profile.size() - 1; ++i)
            gradient[i] = (profile[i+1] - profile[i-1]) / 2.0;
        double maxGrad = 0; int edgeIdx = -1;
        for (size_t i = 1; i < gradient.size() - 1; ++i) {
            bool match = (polarity == 0) ? (std::abs(gradient[i]) >= threshold) :
                         (polarity == 1) ? (gradient[i] <= -threshold) : (gradient[i] >= threshold);
            if (match && std::abs(gradient[i]) > maxGrad) { maxGrad = std::abs(gradient[i]); edgeIdx = (int)i; }
        }
        if (edgeIdx >= 0) edgePoints.push_back(cv::Point2f((float)(rx + edgeIdx), (float)(ry + scanY)));
    }
    if (edgePoints.size() < 2) { setResultData("anomalyCount", (int)edgePoints.size()); setStatus(ToolStatus::NG); return false; }
    // 拟合直线
    cv::Vec4f line;
    cv::fitLine(edgePoints, line, cv::DIST_L2, 0, 0.01, 0.01);
    double angle = std::atan2(line[1], line[0]) * 180.0 / M_PI;
    double centerX = line[2];
    double centerY = line[3];
    setResultData("centerX", centerX);
    setResultData("centerY", centerY);
    setResultData("angle", angle);
    setResultData("anomalyCount", (int)edgePoints.size());
    setResultData("edgeCount", (int)edgePoints.size());
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(LineDetection, "检测直线", VisionInspector::ToolCategory::Detection)
