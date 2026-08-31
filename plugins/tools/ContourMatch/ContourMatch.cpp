#include "ContourMatch.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/geometry/2d.hpp>

#endif

namespace VisionInspector {

PropertyDefList ContourMatch::propertyDefs() const {
    return {
        PropertyDef::stringProp("templatePath", "模板图像路径", ""),
        PropertyDef::doubleProp("threshold", "匹配阈值", 0.5, 0.0, 1.0),
        PropertyDef::intProp("cannyLow", "Canny低阈值", 30, 1, 255),
        PropertyDef::intProp("cannyHigh", "Canny高阈值", 100, 1, 255),
        PropertyDef::intProp("maxMatches", "最大匹配数", 1, 1, 50),
    };
}

#ifdef VI_HAS_OPENCV
static double huMatchScore(const cv::Mat& img1, const cv::Mat& img2) {
    cv::Moments m1 = cv::moments(img1, true);
    cv::Moments m2 = cv::moments(img2, true);
    double hu1[7], hu2[7];
    cv::HuMoments(m1, hu1);
    cv::HuMoments(m2, hu2);
    double score = 0;
    for (int i = 0; i < 7; i++) {
        double v1 = -std::copysign(1.0, hu1[i]) * std::log10(std::abs(hu1[i]) + 1e-10);
        double v2 = -std::copysign(1.0, hu2[i]) * std::log10(std::abs(hu2[i]) + 1e-10);
        score += std::abs(v1 - v2);
    }
    return 1.0 / (1.0 + score);
}
#endif

bool ContourMatch::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    QString templatePath = propertyValue("templatePath").toString();
    double threshold = propertyValue("threshold").toDouble();
    int cannyLow = propertyValue("cannyLow").toInt();
    int cannyHigh = propertyValue("cannyHigh").toInt();
    int maxMatches = propertyValue("maxMatches").toInt();
    cv::Mat templ = cachedTemplateImage(templatePath, cv::IMREAD_GRAYSCALE);   // #8 模板缓存
    if (templ.empty()) { setStatus(ToolStatus::NG); setResultData("error", "无法加载模板"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    cv::Mat templEdge, srcEdge;
    cv::Canny(templ, templEdge, cannyLow, cannyHigh);
    cv::Canny(src, srcEdge, cannyLow, cannyHigh);
    std::vector<std::vector<cv::Point>> templContours, srcContours;
    cv::findContours(templEdge, templContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    cv::findContours(srcEdge, srcContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (templContours.empty()) { setStatus(ToolStatus::NG); setResultData("error", "模板未找到轮廓"); return false; }
    auto tmplContour = *std::max_element(templContours.begin(), templContours.end(),
        [](const auto& a, const auto& b) { return cv::contourArea(a) < cv::contourArea(b); });
    double tmplArea = cv::contourArea(tmplContour);
    struct Match { double x; double y; double score; };
    std::vector<Match> matches;
    for (const auto& sc : srcContours) {
        double srcArea = cv::contourArea(sc);
        if (srcArea < tmplArea * 0.3 || srcArea > tmplArea * 3.0) continue;
        cv::Mat tmplMask = cv::Mat::zeros(templ.size(), CV_8UC1);
        cv::Mat srcMask = cv::Mat::zeros(src.size(), CV_8UC1);
        cv::drawContours(tmplMask, std::vector<std::vector<cv::Point>>{tmplContour}, -1, cv::Scalar(255), -1);
        cv::drawContours(srcMask, std::vector<std::vector<cv::Point>>{sc}, -1, cv::Scalar(255), -1);
        double score = huMatchScore(tmplMask, srcMask);
        if (score >= threshold) { cv::Moments m = cv::moments(sc); if (m.m00 > 0) matches.push_back({m.m10/m.m00, m.m01/m.m00, score}); }
    }
    std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
    if ((int)matches.size() > maxMatches) matches.resize(maxMatches);
    setResultData("matchCount", (int)matches.size());
    if (!matches.empty()) { setResultData("matchX", matches[0].x); setResultData("matchY", matches[0].y); setResultData("score", matches[0].score); setResultData("found", true); setStatus(ToolStatus::OK); }
    else { setResultData("found", false); setResultData("score", 0.0); setStatus(ToolStatus::NG); }
    return !matches.empty();
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}


VI_REGISTER_TOOL(ContourMatch, "轮廓匹配", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector