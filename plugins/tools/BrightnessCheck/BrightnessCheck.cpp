#include "BrightnessCheck.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <cmath>

namespace VisionInspector {

PropertyDefList BrightnessCheck::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形", "圆形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        PropertyDef::intProp("lowThreshold", "下限阈值", 0, 0, 255),
        PropertyDef::intProp("highThreshold", "上限阈值", 255, 0, 255),
    };
}

bool BrightnessCheck::execute(ToolContext& context) {
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
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); return false; }
    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh));
    cv::Scalar mean = cv::mean(roiImg);
    cv::Scalar stddev;
    cv::meanStdDev(roiImg, mean, stddev);
    double avgBrightness = mean[0];
    double stdDeviation = stddev[0];
    double minB = propertyValue("lowThreshold").toDouble();
    double maxB = propertyValue("highThreshold").toDouble();
    bool ok = (avgBrightness >= minB && avgBrightness <= maxB);
    setResultData("avgBrightness", avgBrightness);
    setResultData("stdDeviation", stdDeviation);
    setResultData("minAllowed", minB);
    setResultData("maxAllowed", maxB);
    setResultData("found", true);
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(BrightnessCheck, "检测亮度", VisionInspector::ToolCategory::Detection)
