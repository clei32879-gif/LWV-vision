#include "OCR.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif

namespace VisionInspector {

PropertyDefList OCR::propertyDefs() const {
    return {
        PropertyDef::enumProp("charType", "字符类型", {"全部", "数字", "字母", "数字+字母"}, 0),
        PropertyDef::boolProp("useROI", "使用ROI", false),
        PropertyDef::intProp("roiX", "ROI X", 0, 0, 10000),
        PropertyDef::intProp("roiY", "ROI Y", 0, 0, 10000),
        PropertyDef::intProp("roiW", "ROI 宽度", 0, 0, 10000),
        PropertyDef::intProp("roiH", "ROI 高度", 0, 0, 10000),
        PropertyDef::intProp("minCharHeight", "最小字符高度(px)", 10, 2, 500),
        PropertyDef::intProp("thresholdValue", "二值化阈值", 128, 0, 255),
    };
}

bool OCR::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    cv::Mat roi = src;
    if (propertyValue("useROI").toBool()) {
        int x = propertyValue("roiX").toInt(), y = propertyValue("roiY").toInt();
        int w = propertyValue("roiW").toInt(), h = propertyValue("roiH").toInt();
        if (w > 0 && h > 0) roi = src(cv::Rect(x, y, std::min(w, src.cols - x), std::min(h, src.rows - y)));
    }
    int threshVal = propertyValue("thresholdValue").toInt();
    int minH = propertyValue("minCharHeight").toInt();
    cv::Mat binary;
    cv::threshold(roi, binary, threshVal, 255, cv::THRESH_BINARY_INV);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    int charCount = 0;
    for (const auto& c : contours) { cv::Rect r = cv::boundingRect(c); if (r.height >= minH && r.width >= 2) charCount++; }
    setResultData("charCount", charCount);
    setResultData("recognizedText", QString("(检测到%1个字符区域，需集成Tesseract)").arg(charCount));
    setStatus(charCount > 0 ? ToolStatus::OK : ToolStatus::NG);
    return charCount > 0;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}


VI_REGISTER_TOOL(OCR, "字符识别", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector