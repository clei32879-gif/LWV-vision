#include "ImageCompare.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <opencv2/geometry/2d.hpp>
#endif

namespace VisionInspector {

PropertyDefList ImageCompare::propertyDefs() const {
    return {
        PropertyDef::stringProp("templatePath", "模板图像路径", ""),
        PropertyDef::doubleProp("threshold", "差异阈值", 30.0, 1.0, 255.0),
        PropertyDef::doubleProp("minArea", "最小缺陷面积", 50, 0, 100000),
        PropertyDef::enumProp("diffType", "差异类型", {"暗差异", "亮差异", "两者都检测"}, 2),
        PropertyDef::intProp("edgeMask", "边缘屏蔽宽度", 5, 0, 100),
    };
}

bool ImageCompare::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    QString templatePath = propertyValue("templatePath").toString();
    cv::Mat templ = cachedTemplateImage(templatePath, cv::IMREAD_GRAYSCALE);   // #8 模板缓存
    if (templ.empty()) { setStatus(ToolStatus::NG); setResultData("error", "无法加载模板"); return false; }

    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    // 调整大小匹配
    if (src.size() != templ.size()) {
        cv::resize(src, src, templ.size());
    }

    double threshold = propertyValue("threshold").toDouble();
    double minArea = propertyValue("minArea").toDouble();
    int diffType = propertyValue("diffType").toInt();
    int edgeMask = propertyValue("edgeMask").toInt();

    // 计算差异
    cv::Mat diff;
    cv::absdiff(src, templ, diff);

    // 屏蔽边缘
    if (edgeMask > 0) {
        diff(cv::Rect(0, 0, diff.cols, edgeMask)).setTo(0);
        diff(cv::Rect(0, diff.rows - edgeMask, diff.cols, edgeMask)).setTo(0);
        diff(cv::Rect(0, 0, edgeMask, diff.rows)).setTo(0);
        diff(cv::Rect(diff.cols - edgeMask, 0, edgeMask, diff.rows)).setTo(0);
    }

    // 二值化差异图像
    cv::Mat binary;
    switch (diffType) {
    case 0: // 暗差异 (模板比当前亮)
        cv::threshold(diff, binary, threshold, 255, cv::THRESH_BINARY);
        break;
    case 1: // 亮差异 (模板比当前暗)
        cv::threshold(diff, binary, threshold, 255, cv::THRESH_BINARY);
        break;
    case 2: // 两者都检测
    default:
        cv::threshold(diff, binary, threshold, 255, cv::THRESH_BINARY);
        break;
    }

    // 查找缺陷区域
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    int defectCount = 0;
    double totalDefectArea = 0;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area >= minArea) {
            defectCount++;
            totalDefectArea += area;
        }
    }

    bool passed = (defectCount == 0);

    setResultData("defectCount", defectCount);
    setResultData("totalDefectArea", totalDefectArea);
    setResultData("passed", passed);

    // 输出差异图像
    setOutputImage(context, std::make_shared<CvImage>(binary.clone()));

    setStatus(passed ? ToolStatus::OK : ToolStatus::NG);
    return passed;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ImageCompare, "图像对比", VisionInspector::ToolCategory::Detection)
