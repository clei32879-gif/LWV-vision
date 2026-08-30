#include "ColorDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif

namespace VisionInspector {

PropertyDefList ColorDetection::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形", "圆形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        PropertyDef::enumProp("colorSpace", "颜色空间", {"RGB", "HSV", "LAB"}, 1),
        PropertyDef::intProp("hueMin", "色相最小值", 0, 0, 180),
        PropertyDef::intProp("hueMax", "色相最大值", 180, 0, 180),
        PropertyDef::intProp("satMin", "饱和度最小值", 0, 0, 255),
        PropertyDef::intProp("satMax", "饱和度最大值", 255, 0, 255),
        PropertyDef::intProp("valMin", "明度最小值", 0, 0, 255),
        PropertyDef::intProp("valMax", "明度最大值", 255, 0, 255),
        PropertyDef::doubleProp("minArea", "最小面积", 100, 0, 100000),
    };
}

bool ColorDetection::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() == 1) cv::cvtColor(*input, src, cv::COLOR_GRAY2BGR);
    else src = *input;
    QMap<QString, QVariant> roiProps;
    roiProps["roiType"] = propertyValue("roiType").toInt();
    m_roi = ROIRegion::fromProperties(roiProps);
    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.width = propertyValue("roiWidth").toDouble();
    m_roi.height = propertyValue("roiHeight").toDouble();
    {
        double ang = 0;   // ColorDetection 无角度
        applyCorrection(context, m_roi.centerX, m_roi.centerY, ang);   // 位置补正跟随
    }
    int colorSpace = propertyValue("colorSpace").toInt();
    int hueMin = propertyValue("hueMin").toInt();
    int hueMax = propertyValue("hueMax").toInt();
    int satMin = propertyValue("satMin").toInt();
    int satMax = propertyValue("satMax").toInt();
    int valMin = propertyValue("valMin").toInt();
    int valMax = propertyValue("valMax").toInt();
    double minArea = propertyValue("minArea").toDouble();
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); return false; }
    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    cv::Mat hsv;
    cv::cvtColor(roiImg, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask;
    cv::inRange(hsv, cv::Scalar(hueMin, satMin, valMin), cv::Scalar(hueMax, satMax, valMax), mask);
    // 形态学操作清理
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    // 查找连通域
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    int colorCount = 0;
    double totalArea = 0;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area >= minArea) { colorCount++; totalArea += area; }
    }
    setResultData("colorCount", colorCount);
    setResultData("totalArea", totalArea);
    setResultData("found", colorCount > 0);
    setStatus(colorCount > 0 ? ToolStatus::OK : ToolStatus::NG);
    return colorCount > 0;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ColorDetection, "颜色识别", VisionInspector::ToolCategory::Detection)
