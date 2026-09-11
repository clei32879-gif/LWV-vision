#include "ImageProcess.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList ImageProcess::propertyDefs() const {
    return {
        PropertyDef::enumProp("operation", "处理操作",
            {"灰度转换", "二值化", "反色", "亮度增强", "对比度增强", "反二值化", "OTSU自动二值化", "自适应二值化"}, 0),
        PropertyDef::intProp("threshold", "二值化阈值", 128, 0, 255),
        PropertyDef::intProp("blockSize", "自适应块大小(奇数)", 11, 3, 255),
        PropertyDef::doubleProp("brightness", "亮度增益", 1.0, 0.1, 5.0),
        PropertyDef::doubleProp("contrast", "对比度增益", 1.0, 0.1, 5.0),
    };
}

bool ImageProcess::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    int op = propertyValue("operation").toInt();
    cv::Mat result;

    switch (op) {
    case 0: // 灰度转换
        if (input->channels() > 1) cv::cvtColor(*input, result, cv::COLOR_BGR2GRAY);
        else result = *input;
        break;
    case 1: // 二值化
        { cv::Mat gray;
          if (input->channels() > 1) cv::cvtColor(*input, gray, cv::COLOR_BGR2GRAY);
          else gray = *input;
          cv::threshold(gray, result, propertyValue("threshold").toInt(), 255, cv::THRESH_BINARY);
        }
        break;
    case 2: // 反色
        cv::bitwise_not(*input, result);
        break;
    case 3: // 亮度增强
        { double b = propertyValue("brightness").toDouble();
          input->convertTo(result, -1, b, 0); }
        break;
    case 4: // 对比度增强
        { double c = propertyValue("contrast").toDouble();
          input->convertTo(result, -1, c, 0); }
        break;
    default:
        result = *input;
        break;
    }

    setOutputImage(context, std::make_shared<CvImage>(result.clone()));
    setResultData("outputWidth", result.cols);
    setResultData("outputHeight", result.rows);
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ImageProcess, "图像处理", VisionInspector::ToolCategory::ImageProcess)
