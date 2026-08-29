#include "Threshold.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList Threshold::propertyDefs() const {
    return {
        PropertyDef::enumProp("method", "阈值方法", {"固定阈值", "OTSU自动阈值", "自适应均值", "自适应高斯"}, 1),
        PropertyDef::intProp("threshValue", "固定阈值", 128, 0, 255),
        PropertyDef::enumProp("threshType", "阈值类型", {"二值化", "反二值化"}, 0),
        PropertyDef::intProp("blockSize", "自适应块大小", 11, 3, 99),
        PropertyDef::intProp("C", "自适应常数C", 2, -10, 10),
    };
}

bool Threshold::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    int method = propertyValue("method").toInt();
    int threshType = propertyValue("threshType").toInt() == 0 ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;
    int blockSize = propertyValue("blockSize").toInt();
    int C = propertyValue("C").toInt();
    if (blockSize % 2 == 0) blockSize++;
    cv::Mat result;
    switch (method) {
    case 0: cv::threshold(src, result, propertyValue("threshValue").toInt(), 255, threshType); break;
    case 1: cv::threshold(src, result, 0, 255, threshType | cv::THRESH_OTSU); break;
    case 2: cv::adaptiveThreshold(src, result, 255, cv::ADAPTIVE_THRESH_MEAN_C, threshType, blockSize, C); break;
    case 3: cv::adaptiveThreshold(src, result, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, threshType, blockSize, C); break;
    default: result = src; break;
    }
    setOutputImage(context, std::make_shared<CvImage>(result.clone()));
    setStatus(ToolStatus::OK); return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}


VI_REGISTER_TOOL(Threshold, "阈值分割", VisionInspector::ToolCategory::ImageProcess)
} // namespace VisionInspector