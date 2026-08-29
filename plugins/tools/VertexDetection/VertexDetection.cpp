#include "VertexDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif
#include <cmath>

namespace VisionInspector {

PropertyDefList VertexDetection::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形", "圆形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0),
        PropertyDef::enumProp("detectPosition", "检测位置", {"起始", "最近", "最远", "最强"}, 0),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 40, 1, 255),
        PropertyDef::intProp("filterHalfWidth", "滤波半宽", 1, 1, 20),
        PropertyDef::intProp("scanWidth", "扫描宽度", 1, 1, 100),
    };
}

bool VertexDetection::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    QMap<QString, QVariant> roiProps;
    roiProps["roiType"] = propertyValue("roiType").toInt();
    m_roi = ROIRegion::fromProperties(roiProps);
    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.width = propertyValue("roiWidth").toDouble();
    m_roi.height = propertyValue("roiHeight").toDouble();
    int polarity = propertyValue("edgePolarity").toInt();
    int threshold = propertyValue("gradientThreshold").toInt();
    int scanWidth = propertyValue("scanWidth").toInt();
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); return false; }
    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    // 在ROI中心十字扫描找角点
    double bestX = 0, bestY = 0;
    double maxScore = 0;
    for (int x = rw/4; x < rw*3/4; ++x) {
        for (int y = rh/4; y < rh*3/4; y += 2) {
            // 计算Harris响应
            double dx = (x < rw-1) ? (double)roiImg.at<uchar>(y, x+1) - roiImg.at<uchar>(y, x) : 0;
            double dy = (y < rh-1) ? (double)roiImg.at<uchar>(y+1, x) - roiImg.at<uchar>(y, x) : 0;
            double score = dx*dx + dy*dy;
            if (score > maxScore) { maxScore = score; bestX = x; bestY = y; }
        }
    }
    if (maxScore > threshold * threshold) {
        setResultData("positionX", rx + bestX);
        setResultData("positionY", ry + bestY);
        setResultData("distance", std::sqrt(maxScore));
        setResultData("found", true);
        setStatus(ToolStatus::OK);
        return true;
    }
    setResultData("found", false);
    setStatus(ToolStatus::NG);
    return false;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(VertexDetection, "检测顶点", VisionInspector::ToolCategory::Detection)
