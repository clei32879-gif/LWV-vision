#include "ImageCorrection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <cmath>

namespace VisionInspector {

PropertyDefList ImageCorrection::propertyDefs() const {
    return {
        PropertyDef::enumProp("correctionType", "补正类型", {"亮度均匀化", "对比度增强", "白平衡"}, 0),
        PropertyDef::doubleProp("brightness", "亮度增益", 1.0, 0.1, 5.0),
        PropertyDef::doubleProp("contrast", "对比度增益", 1.0, 0.1, 5.0),
        PropertyDef::intProp("blurSize", "模糊核大小", 51, 3, 201),
    };
}

bool ImageCorrection::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    int type = propertyValue("correctionType").toInt();
    cv::Mat result;
    switch (type) {
    case 0: { // 亮度均匀化
        double blurSize = propertyValue("blurSize").toDouble();
        if ((int)blurSize % 2 == 0) blurSize += 1;
        cv::Mat gray;
        if (input->channels() > 1) cv::cvtColor(*input, gray, cv::COLOR_BGR2GRAY);
        else gray = *input;
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size((int)blurSize, (int)blurSize), 0);
        cv::Mat corrected;
        cv::divide(gray, blurred, corrected, 255);
        if (input->channels() > 1) cv::cvtColor(corrected, result, cv::COLOR_GRAY2BGR);
        else result = corrected;
        break;
    }
    case 1: { // 对比度增强
        double b = propertyValue("brightness").toDouble();
        double c = propertyValue("contrast").toDouble();
        input->convertTo(result, -1, c, (b - 1.0) * 128);
        break;
    }
    case 2: { // 白平衡（灰度世界假设）
        if (input->channels() == 3) {
            cv::Scalar mean = cv::mean(*input);
            double avg = (mean[0] + mean[1] + mean[2]) / 3.0;
            result = input->clone();
            for (int y = 0; y < result.rows; ++y) {
                for (int x = 0; x < result.cols; ++x) {
                    cv::Vec3b& pixel = result.at<cv::Vec3b>(y, x);
                    for (int c = 0; c < 3; ++c) {
                        pixel[c] = cv::saturate_cast<uchar>(pixel[c] * avg / (mean[c] > 0 ? mean[c] : 1));
                    }
                }
            }
        } else {
            result = *input;
        }
        break;
    }
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

VI_REGISTER_TOOL(ImageCorrection, "图像补正", VisionInspector::ToolCategory::Calibration)
