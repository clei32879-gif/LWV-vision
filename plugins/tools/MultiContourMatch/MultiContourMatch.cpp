#include "MultiContourMatch.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif

namespace VisionInspector {

PropertyDefList MultiContourMatch::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        PropertyDef::doubleProp("minArea", "最小面积", 100, 1, 100000),
        PropertyDef::doubleProp("maxArea", "最大面积", 50000, 1, 1000000),
        PropertyDef::intProp("minCount", "最少数量", 1, 1, 100),
        PropertyDef::doubleProp("sizeTolerance", "尺寸容差(%)", 30, 0, 100),
    };
}

bool MultiContourMatch::execute(ToolContext& context) {
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
        double ang = 0;   // MultiContourMatch 无角度
        applyCorrection(context, m_roi.centerX, m_roi.centerY, ang);   // 位置补正跟随
    }
    double minArea = propertyValue("minArea").toDouble();
    double maxArea = propertyValue("maxArea").toDouble();
    int minCount = propertyValue("minCount").toInt();
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); return false; }
    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    cv::Mat binary;
    cv::threshold(roiImg, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    int matchCount = 0;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area >= minArea && area <= maxArea) matchCount++;
    }
    setResultData("matchCount", matchCount);
    setResultData("totalContours", (int)contours.size());
    setResultData("found", matchCount >= minCount);
    setStatus(matchCount >= minCount ? ToolStatus::OK : ToolStatus::NG);
    return matchCount >= minCount;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(MultiContourMatch, "多轮廓匹配", VisionInspector::ToolCategory::Detection)
