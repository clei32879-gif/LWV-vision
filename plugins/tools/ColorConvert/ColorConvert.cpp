#include "ColorConvert.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList ColorConvert::propertyDefs() const {
    return {
        PropertyDef::enumProp("targetFormat", "目标格式", {"灰度", "HSV", "LAB", "BGR"}, 0),
    };
}

bool ColorConvert::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    int format = propertyValue("targetFormat").toInt();
    cv::Mat result;
    // M-40修复: 灰度图转HSV/LAB时先复制3通道再转换, 而非停留在BGR
    cv::Mat bgr;
    if (input->channels() == 1)
        cv::cvtColor(*input, bgr, cv::COLOR_GRAY2BGR);
    else
        bgr = *input;
    switch (format) {
    case 0: if (input->channels() > 1) cv::cvtColor(*input, result, cv::COLOR_BGR2GRAY); else result = *input; break;
    case 1: cv::cvtColor(bgr, result, cv::COLOR_BGR2HSV); break;
    case 2: cv::cvtColor(bgr, result, cv::COLOR_BGR2Lab); break;
    case 3: result = bgr; break;
    default: result = *input; break;
    }
    setOutputImage(context, std::make_shared<CvImage>(result.clone()));
    setResultData("outputChannels", result.channels());
    setStatus(ToolStatus::OK); return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}


VI_REGISTER_TOOL(ColorConvert, "颜色空间转换", VisionInspector::ToolCategory::ImageProcess)
} // namespace VisionInspector