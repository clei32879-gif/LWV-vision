#include "BarcodeReader.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList BarcodeReader::propertyDefs() const {
    return {
        PropertyDef::enumProp("barcodeType", "条码类型", {"自动检测", "Code128", "Code39", "EAN13"}, 0),
        PropertyDef::boolProp("useROI", "使用ROI", false),
        PropertyDef::intProp("roiX", "ROI X", 0, 0, 10000),
        PropertyDef::intProp("roiY", "ROI Y", 0, 0, 10000),
        PropertyDef::intProp("roiW", "ROI 宽度", 0, 0, 10000),
        PropertyDef::intProp("roiH", "ROI 高度", 0, 0, 10000),
    };
}

bool BarcodeReader::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    setResultData("barcodeDetected", false);
    setResultData("barcodeText", "");
    setResultData("barcodeType", "需集成条码解码库");
    setStatus(ToolStatus::NG);
    return false;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}


VI_REGISTER_TOOL(BarcodeReader, "条码识别", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector