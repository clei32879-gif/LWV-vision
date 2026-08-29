#include "ShapeMatch.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/geometry/2d.hpp>

#endif

namespace VisionInspector {

PropertyDefList ShapeMatch::propertyDefs() const {
    return {
        PropertyDef::stringProp("templatePath", "模板图像路径", ""),
        PropertyDef::doubleProp("threshold", "匹配阈值", 0.6, 0.0, 1.0),
        PropertyDef::intProp("maxMatches", "最大匹配数", 1, 1, 100),
        PropertyDef::doubleProp("angleRange", "角度搜索范围(度)", 30.0, 0.0, 180.0),
        PropertyDef::doubleProp("scaleRange", "缩放搜索范围", 0.2, 0.0, 1.0),
        PropertyDef::intProp("cannyLow", "Canny低阈值", 50, 1, 255),
        PropertyDef::intProp("cannyHigh", "Canny高阈值", 150, 1, 255),
    };
}

#ifdef VI_HAS_OPENCV
static double contourMatchScore(const std::vector<cv::Point>& tmpl, const std::vector<cv::Point>& target) {
    if (tmpl.empty() || target.empty()) return 0.0;
    double score = cv::matchShapes(tmpl, target, cv::CONTOURS_MATCH_I1, 0);
    return 1.0 / (1.0 + score);
}
#endif

bool ShapeMatch::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    QString templatePath = propertyValue("templatePath").toString();
    double threshold = propertyValue("threshold").toDouble();
    int maxMatches = propertyValue("maxMatches").toInt();
    int cannyLow = propertyValue("cannyLow").toInt();
    int cannyHigh = propertyValue("cannyHigh").toInt();
    cv::Mat templ = cv::imread(templatePath.toUtf8().constData(), cv::IMREAD_GRAYSCALE);
    if (templ.empty()) { setStatus(ToolStatus::NG); setResultData("error", "无法加载模板"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    cv::Mat templBin;
    cv::threshold(templ, templBin, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    std::vector<std::vector<cv::Point>> templContours;
    cv::findContours(templBin, templContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (templContours.empty()) { setStatus(ToolStatus::NG); setResultData("error", "模板未找到轮廓"); return false; }
    auto tmplContour = *std::max_element(templContours.begin(), templContours.end(),
        [](const auto& a, const auto& b) { return cv::contourArea(a) < cv::contourArea(b); });
    cv::Mat srcBin;
    cv::Canny(src, srcBin, cannyLow, cannyHigh);
    cv::dilate(srcBin, srcBin, cv::Mat(), cv::Point(-1,-1), 1);
    std::vector<std::vector<cv::Point>> srcContours;
    cv::findContours(srcBin, srcContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    struct Match { double x; double y; double score; };
    std::vector<Match> matches;
    for (const auto& sc : srcContours) {
        if (cv::contourArea(sc) < 100) continue;
        double score = contourMatchScore(tmplContour, sc);
        if (score >= threshold) {
            cv::Moments m = cv::moments(sc);
            if (m.m00 > 0) matches.push_back({m.m10/m.m00, m.m01/m.m00, score});
        }
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


VI_REGISTER_TOOL(ShapeMatch, "形状匹配", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector