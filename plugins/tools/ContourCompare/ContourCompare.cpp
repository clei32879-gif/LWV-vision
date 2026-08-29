#include "ContourCompare.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/geometry/2d.hpp>
#endif

namespace VisionInspector {

PropertyDefList ContourCompare::propertyDefs() const {
    return {
        PropertyDef::stringProp("templatePath", "模板图像路径", ""),
        PropertyDef::doubleProp("threshold", "匹配阈值", 0.7, 0.0, 1.0),
        PropertyDef::doubleProp("minArea", "最小面积", 100, 0, 100000),
    };
}

bool ContourCompare::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    QString templatePath = propertyValue("templatePath").toString();
    double threshold = propertyValue("threshold").toDouble();
    double minArea = propertyValue("minArea").toDouble();
    cv::Mat templ = cv::imread(templatePath.toUtf8().constData(), cv::IMREAD_GRAYSCALE);
    if (templ.empty()) { setStatus(ToolStatus::NG); setResultData("error", "无法加载模板"); return false; }
    // 提取模板轮廓
    cv::Mat templBin;
    cv::threshold(templ, templBin, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    std::vector<std::vector<cv::Point>> templContours;
    cv::findContours(templBin, templContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (templContours.empty()) { setStatus(ToolStatus::NG); return false; }
    auto tmplContour = *std::max_element(templContours.begin(), templContours.end(),
        [](const auto& a, const auto& b) { return cv::contourArea(a) < cv::contourArea(b); });
    // 提取输入图像轮廓
    cv::Mat srcBin;
    cv::threshold(src, srcBin, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    std::vector<std::vector<cv::Point>> srcContours;
    cv::findContours(srcBin, srcContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    // 对比轮廓
    double bestScore = 0;
    int bestIdx = -1;
    for (int i = 0; i < (int)srcContours.size(); ++i) {
        if (cv::contourArea(srcContours[i]) < minArea) continue;
        double score = cv::matchShapes(tmplContour, srcContours[i], cv::CONTOURS_MATCH_I1, 0);
        double similarity = 1.0 / (1.0 + score);
        if (similarity > bestScore) { bestScore = similarity; bestIdx = i; }
    }
    bool found = bestScore >= threshold;
    setResultData("score", bestScore);
    setResultData("found", found);
    setResultData("matchCount", bestIdx >= 0 ? 1 : 0);
    setStatus(found ? ToolStatus::OK : ToolStatus::NG);
    return found;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ContourCompare, "轮廓对比", VisionInspector::ToolCategory::Detection)
