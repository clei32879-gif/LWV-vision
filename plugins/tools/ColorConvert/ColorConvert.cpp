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
    switch (format) {
    case 0: if (input->channels() > 1) cv::cvtColor(*input, result, cv::COLOR_BGR2GRAY); else result = *input; break;
    case 1: if (input->channels() > 1) cv::cvtColor(*input, result, cv::COLOR_BGR2HSV); else cv::cvtColor(*input, result, cv::COLOR_GRAY2BGR); break;
    case 2: if (input->channels() > 1) cv::cvtColor(*input, result, cv::COLOR_BGR2Lab); else cv::cvtColor(*input, result, cv::COLOR_GRAY2BGR); break;
    case 3: result = *input; break;
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