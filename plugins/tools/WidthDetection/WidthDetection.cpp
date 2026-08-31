#include "WidthDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif
#include <cmath>

namespace VisionInspector {

PropertyDefList WidthDetection::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        PropertyDef::intProp("threshold", "阈值", 128, 0, 255),
        PropertyDef::enumProp("measureMode", "测量模式", {"水平宽度", "垂直宽度"}, 0),
        PropertyDef::intProp("scanLine", "扫描行号", 0, 0, 10000),
    };
}

bool WidthDetection::execute(ToolContext& context) {
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
    {
        double ang = 0;   // WidthDetection 无角度
        applyCorrection(context, m_roi.centerX, m_roi.centerY, ang);   // 位置补正跟随
    }
    int thresh = propertyValue("threshold").toInt();
    int mode = propertyValue("measureMode").toInt();
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); return false; }
    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    cv::Mat binary;
    cv::threshold(roiImg, binary, thresh, 255, cv::THRESH_BINARY);
    // #2 形状ROI: 菱形/圆形ROI限制宽度测量范围
    {
        const cv::Mat shapeMask = makeRoiShapeMask(m_roi, roiRect);
        if (!shapeMask.empty())
            cv::bitwise_and(binary, shapeMask, binary);
    }
    if (mode == 0) { // 水平宽度
        int scanY = propertyValue("scanLine").toInt();
        if (scanY >= rh) scanY = rh / 2;
        int leftEdge = -1, rightEdge = -1;
        for (int x = 0; x < rw; ++x) {
            if (binary.at<uchar>(scanY, x) > 0 && leftEdge < 0) leftEdge = x;
            if (binary.at<uchar>(scanY, x) > 0) rightEdge = x;
        }
        double width = (leftEdge >= 0 && rightEdge >= 0) ? (rightEdge - leftEdge) : 0;
        setResultData("width", width);
        setResultData("leftEdge", rx + leftEdge);
        setResultData("rightEdge", rx + rightEdge);
        setResultData("found", width > 0);
        setStatus(width > 0 ? ToolStatus::OK : ToolStatus::NG);
        return width > 0;
    } else { // 垂直宽度
        int scanX = rw / 2;
        int topEdge = -1, bottomEdge = -1;
        for (int y = 0; y < rh; ++y) {
            if (binary.at<uchar>(y, scanX) > 0 && topEdge < 0) topEdge = y;
            if (binary.at<uchar>(y, scanX) > 0) bottomEdge = y;
        }
        double height = (topEdge >= 0 && bottomEdge >= 0) ? (bottomEdge - topEdge) : 0;
        setResultData("width", height);
        setResultData("found", height > 0);
        setStatus(height > 0 ? ToolStatus::OK : ToolStatus::NG);
        return height > 0;
    }
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(WidthDetection, "检测宽窄", VisionInspector::ToolCategory::Detection)
