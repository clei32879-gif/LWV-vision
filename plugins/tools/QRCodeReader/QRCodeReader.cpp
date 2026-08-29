#include "QRCodeReader.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#endif

namespace VisionInspector {

PropertyDefList QRCodeReader::propertyDefs() const {
    return {
        PropertyDef::boolProp("useROI", "使用ROI", false),
        PropertyDef::intProp("roiX", "ROI X", 0, 0, 10000),
        PropertyDef::intProp("roiY", "ROI Y", 0, 0, 10000),
        PropertyDef::intProp("roiW", "ROI 宽度", 0, 0, 10000),
        PropertyDef::intProp("roiH", "ROI 高度", 0, 0, 10000),
    };
}

bool QRCodeReader::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    cv::Mat roi = src;
    int offsetX = 0, offsetY = 0;
    if (propertyValue("useROI").toBool()) {
        int x = propertyValue("roiX").toInt(), y = propertyValue("roiY").toInt();
        int w = propertyValue("roiW").toInt(), h = propertyValue("roiH").toInt();
        if (w > 0 && h > 0) { roi = src(cv::Rect(x, y, std::min(w, src.cols - x), std::min(h, src.rows - y))); offsetX = x; offsetY = y; }
    }

    cv::QRCodeDetector detector;
    std::vector<cv::Point> points;
    // OpenCV 5.0: detectAndDecode 返回解码文本，points作为输出参数
    std::string decodedText = detector.detectAndDecode(roi, points);

    if (!decodedText.empty()) {
        setResultData("decoded", true);
        setResultData("text", QString::fromStdString(decodedText));
        if (points.size() >= 4) {
            setResultData("centerX", (points[0].x + points[2].x) / 2.0 + offsetX);
            setResultData("centerY", (points[0].y + points[2].y) / 2.0 + offsetY);
        }
        setStatus(ToolStatus::OK);
        return true;
    }
    setResultData("decoded", false);
    setResultData("text", "");
    setStatus(ToolStatus::NG);
    return false;
#else
    setResultData("error", "需要OpenCV库");
    setStatus(ToolStatus::NG);
    return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(QRCodeReader, "二维码识别", VisionInspector::ToolCategory::Detection)
