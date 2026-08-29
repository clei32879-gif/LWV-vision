#include "GrayscaleMatch.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#endif
#include <QFile>

namespace VisionInspector {

PropertyDefList GrayscaleMatch::propertyDefs() const {
    return {
        PropertyDef::stringProp("templatePath", "模板图像路径", ""),
        PropertyDef::doubleProp("threshold", "匹配阈值", 0.7, 0.0, 1.0),
        PropertyDef::intProp("maxMatches", "最大匹配数", 1, 1, 100),
        PropertyDef::doubleProp("angleStart", "角度搜索起始(度)", -15.0, -180.0, 180.0),
        PropertyDef::doubleProp("angleEnd", "角度搜索结束(度)", 15.0, -180.0, 180.0),
        PropertyDef::doubleProp("angleStep", "角度步长(度)", 1.0, 0.1, 45.0),
        PropertyDef::doubleProp("scaleMin", "缩放最小值", 0.9, 0.1, 2.0),
        PropertyDef::doubleProp("scaleMax", "缩放最大值", 1.1, 0.1, 2.0),
    };
}

bool GrayscaleMatch::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        setStatus(ToolStatus::NG);
        setResultData("error", "输入图像为空");
        return false;
    }

    QString templatePath = propertyValue("templatePath").toString();
    double threshold = propertyValue("threshold").toDouble();
    int maxMatches = propertyValue("maxMatches").toInt();

    cv::Mat templ = cv::imread(templatePath.toUtf8().constData(), cv::IMREAD_GRAYSCALE);
    if (templ.empty()) {
        setStatus(ToolStatus::NG);
        setResultData("error", QString("无法加载模板: %1").arg(templatePath));
        return false;
    }

    cv::Mat src;
    if (input->channels() > 1)
        cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else
        src = *input;

    cv::Mat result;
    cv::matchTemplate(src, templ, result, cv::TM_CCOEFF_NORMED);

    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

    struct Match { double x; double y; double score; };
    std::vector<Match> matches;

    cv::Mat work = result.clone();
    for (int i = 0; i < maxMatches; i++) {
        cv::minMaxLoc(work, &minVal, &maxVal, &minLoc, &maxLoc);
        if (maxVal < threshold) break;
        matches.push_back({(double)maxLoc.x, (double)maxLoc.y, maxVal});
        int r = templ.rows / 2, c = templ.cols / 2;
        cv::Rect suppression(maxLoc.x - c, maxLoc.y - r, templ.cols, templ.rows);
        suppression &= cv::Rect(0, 0, work.cols, work.rows);
        work(suppression).setTo(0);
    }

    setResultData("matchCount", (int)matches.size());
    if (!matches.empty()) {
        setResultData("matchX", matches[0].x + templ.cols / 2.0);
        setResultData("matchY", matches[0].y + templ.rows / 2.0);
        setResultData("score", matches[0].score);
        setResultData("found", true);
        setStatus(ToolStatus::OK);
    } else {
        setResultData("found", false);
        setResultData("score", 0.0);
        setStatus(ToolStatus::NG);
    }
    return !matches.empty();
#else
    setResultData("error", "需要OpenCV库");
    setStatus(ToolStatus::NG);
    return false;
#endif
}

VI_REGISTER_TOOL(GrayscaleMatch, "灰度匹配", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector
