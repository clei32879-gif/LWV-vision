#include "ImageFilter.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList ImageFilter::propertyDefs() const {
    return {
        PropertyDef::enumProp("filterType", "滤波类型", {"均值滤波", "中值滤波", "高斯滤波", "双边滤波"}, 2),
        PropertyDef::intProp("kernelSize", "核大小", 3, 1, 31),
        PropertyDef::doubleProp("sigmaX", "Sigma X (高斯)", 1.0, 0.1, 50.0),
        PropertyDef::doubleProp("sigmaColor", "Sigma Color (双边)", 50.0, 1.0, 200.0),
        PropertyDef::doubleProp("sigmaSpace", "Sigma Space (双边)", 50.0, 1.0, 200.0),
    };
}

bool ImageFilter::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    int filterType = propertyValue("filterType").toInt();
    int ksize = propertyValue("kernelSize").toInt();
    if (ksize % 2 == 0) ksize++;
    cv::Mat result;
    switch (filterType) {
    case 0: cv::blur(*input, result, cv::Size(ksize, ksize)); break;
    case 1: cv::medianBlur(*input, result, ksize); break;
    case 2: cv::GaussianBlur(*input, result, cv::Size(ksize, ksize), propertyValue("sigmaX").toDouble()); break;
    case 3: cv::bilateralFilter(*input, result, ksize, propertyValue("sigmaColor").toDouble(), propertyValue("sigmaSpace").toDouble()); break;
    default: result = *input; break;
    }
    setOutputImage(context, std::make_shared<CvImage>(result.clone()));
    setResultData("outputWidth", result.cols); setResultData("outputHeight", result.rows);
    setStatus(ToolStatus::OK); return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}


VI_REGISTER_TOOL(ImageFilter, "图像滤波", VisionInspector::ToolCategory::ImageProcess)
} // namespace VisionInspector