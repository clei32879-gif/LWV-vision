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

// 旋转敏感距离(归一化): 模板与目标轮廓各自质心居中+按面积开方归一尺度,
// 再计算模板点到目标轮廓的最近距离均值 (越小越对齐, 用于定角度; 平移/缩放无关)
static double contourPointDist(const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
    if (a.empty() || b.empty()) return 1e9;
    const cv::Moments ma = cv::moments(a), mb = cv::moments(b);
    if (ma.m00 <= 0 || mb.m00 <= 0) return 1e9;
    const double ax = ma.m10 / ma.m00, ay = ma.m01 / ma.m00;
    const double bx = mb.m10 / mb.m00, by = mb.m01 / mb.m00;
    const double sa = std::sqrt(ma.m00), sb = std::sqrt(mb.m00);
    double sum = 0;
    for (const auto& p : a) {
        const double px = (p.x - ax) / sa, py = (p.y - ay) / sa;
        double best = 1e18;
        for (const auto& q : b) {
            const double qx = (q.x - bx) / sb, qy = (q.y - by) / sb;
            const double d = std::hypot(px - qx, py - qy);
            if (d < best) best = d;
        }
        sum += best;
    }
    return sum / a.size();
}
#endif

bool ShapeMatch::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    QString templatePath = propertyValue("templatePath").toString();
    double threshold = propertyValue("threshold").toDouble();
    int maxMatches = propertyValue("maxMatches").toInt();
    double angleRange = propertyValue("angleRange").toDouble();   // 旋转搜索范围(度)
    double scaleRange = propertyValue("scaleRange").toDouble();   // 缩放搜索范围
    int cannyLow = propertyValue("cannyLow").toInt();
    int cannyHigh = propertyValue("cannyHigh").toInt();
    cv::Mat templ = cv::imread(templatePath.toUtf8().constData(), cv::IMREAD_GRAYSCALE);
    if (templ.empty()) { setStatus(ToolStatus::NG); setResultData("error", "无法加载模板"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    // ---- 旋转/缩放搜索: 生成多个角度的模板轮廓, 逐角度匹配取全局最佳 ----
    // 模板图像以中心旋转后重新取轮廓 (对齐CKVision"角度搜索范围")
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

    struct Match { double x; double y; double score; double angle; double scale; };
    std::vector<Match> matches;

    // 模板质心(旋转中心)
    const cv::Moments tm = cv::moments(tmplContour);
    const double tcx = tm.m00 > 0 ? tm.m10 / tm.m00 : templ.cols / 2.0;
    const double tcy = tm.m00 > 0 ? tm.m01 / tm.m00 : templ.rows / 2.0;
    const double tmplArea = cv::contourArea(tmplContour);

    // 采样角度与缩放
    const double aStep = std::max(1.0, angleRange / 24.0);          // 每角度范围内≤25个采样
    std::vector<double> angles;                                     // 负角度到正角度
    for (double a = -angleRange; a <= angleRange + 1e-9; a += aStep) angles.push_back(a);
    const double sStep = std::max(0.02, scaleRange / 10.0);
    std::vector<double> scales;
    for (double s = 1.0 - scaleRange; s <= 1.0 + scaleRange + 1e-9; s += sStep) scales.push_back(s);

    for (const auto& sc : srcContours) {
        if (cv::contourArea(sc) < 100) continue;
        // 基础形状分数 (旋转/缩放不变的 matchShapes, 用于阈值判定, 保持原语义)
        const double base = contourMatchScore(tmplContour, sc);
        if (base < threshold) continue;

        const cv::Moments sm = cv::moments(sc);
        const double scx = sm.m00 > 0 ? sm.m10 / sm.m00 : 0;
        const double scy = sm.m00 > 0 ? sm.m01 / sm.m00 : 0;
        const double scArea = cv::contourArea(sc);

        // 角度搜索: 旋转敏感的点距最小化定最佳角度 (matchShapes 旋转不变, 对对称形状无区分度)
        double bestAngle = 0, bestDist = 1e18;
        if (angleRange > 0.5) {
            for (double ang : angles) {
                const double rad = ang * CV_PI / 180.0;
                const double c = std::cos(rad), s = std::sin(rad);
                std::vector<cv::Point> rotated;
                rotated.reserve(tmplContour.size());
                for (const auto& p : tmplContour) {
                    const double dx = p.x - tcx, dy = p.y - tcy;
                    rotated.emplace_back((int)std::lround(tcx + dx * c - dy * s),
                                         (int)std::lround(tcy + dx * s + dy * c));
                }
                const double d = contourPointDist(rotated, sc);
                if (d < bestDist) { bestDist = d; bestAngle = ang; }
            }
        }
        const double bestScale = tmplArea > 1e-6 ? scArea / tmplArea : 1.0;
        matches.push_back({scx, scy, base, bestAngle, bestScale});
    }
    std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
    if ((int)matches.size() > maxMatches) matches.resize(maxMatches);
    setResultData("matchCount", (int)matches.size());
    if (!matches.empty()) {
        setResultData("matchX", matches[0].x); setResultData("matchY", matches[0].y);
        setResultData("score", matches[0].score);
        setResultData("matchAngle", matches[0].angle);   // 本次角度搜索得到的最佳旋转角
        setResultData("matchScale", matches[0].scale);
        setResultData("found", true); setStatus(ToolStatus::OK);
    }
    else { setResultData("found", false); setResultData("score", 0.0); setStatus(ToolStatus::NG); }
    return !matches.empty();
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}


VI_REGISTER_TOOL(ShapeMatch, "形状匹配", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector