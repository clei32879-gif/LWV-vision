#include "TemplateMatch.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#endif

namespace VisionInspector {

PropertyDefList TemplateMatch::propertyDefs() const {
    return {
        PropertyDef::stringProp("templatePath", "模板图像路径", ""),
        PropertyDef::enumProp("method", "匹配方法", {"平方差", "归一化平方差", "相关系数", "归一化相关系数"}, 3),
        PropertyDef::doubleProp("threshold", "匹配阈值", 0.8, 0.0, 1.0),
    };
}

bool TemplateMatch::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    QString templatePath = propertyValue("templatePath").toString();
    cv::Mat templ = cv::imread(templatePath.toUtf8().constData(), cv::IMREAD_GRAYSCALE);
    if (templ.empty()) { setStatus(ToolStatus::NG); setResultData("error", "无法加载模板"); return false; }

    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    int method = propertyValue("method").toInt();
    cv::Mat result;
    cv::Point matchLoc;
    try {
        cv::matchTemplate(src, templ, result, method);
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        // H-22修复: SQDIFF/SQDIFF_NORMED(0/1) 最佳匹配是最小值
        if (method <= 1) matchLoc = minLoc;
        else matchLoc = maxLoc;
    } catch (const cv::Exception& e) {
        // H-22: 模板大于原图等异常不再崩溃(属性对话框试运行也无try/catch)
        setStatus(ToolStatus::NG);
        setResultData("error", QString("模板匹配异常: %1").arg(e.what()));
        return false;
    }

    double threshold = propertyValue("threshold").toDouble();
    double minVal, maxVal;
    cv::minMaxLoc(result, &minVal, &maxVal);
    double score = (method <= 1) ? (1.0 - minVal) : maxVal;
    bool found = score >= threshold;

    setResultData("score", score);
    setResultData("found", found);
    if (found) {
        setResultData("matchX", (double)matchLoc.x + templ.cols / 2.0);
        setResultData("matchY", (double)matchLoc.y + templ.rows / 2.0);
    }
    setStatus(found ? ToolStatus::OK : ToolStatus::NG);
    return found;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(TemplateMatch, "模板匹配", VisionInspector::ToolCategory::Detection)
