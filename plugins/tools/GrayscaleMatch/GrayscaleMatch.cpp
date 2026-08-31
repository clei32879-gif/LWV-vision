#include "GrayscaleMatch.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
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
    // 旋转/缩放搜索 (M-43: 让搜索参数真正生效)
    const double angleStart = propertyValue("angleStart").toDouble();
    const double angleEnd = propertyValue("angleEnd").toDouble();
    const double angleStep = std::max(0.1, propertyValue("angleStep").toDouble());
    const double scaleMin = std::max(0.1, propertyValue("scaleMin").toDouble());
    const double scaleMax = std::max(scaleMin, propertyValue("scaleMax").toDouble());

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

    // ---- 角度/缩放搜索: 金字塔粗到细 (#7 性能) ----
    // 粗阶段: 1/4分辨率跑全部角度×缩放 (matchTemplate代价降16倍)
    // 细阶段: 只对粗阶段最优的少数候选, 在原分辨率局部窗口内精搜
    struct Match { double x; double y; double score; double angle; double scale; };
    std::vector<Match> matches;

    // 对模板做旋转(绕模板中心), 保留背景填充边界
    const int tw = templ.cols, th = templ.rows;
    const double dia = std::ceil(std::hypot(tw, th));
    const cv::Size canv((int)dia, (int)dia);

    std::vector<double> angles;
    for (double a = angleStart; a <= angleEnd + 1e-9; a += angleStep) angles.push_back(a);
    std::vector<double> scales;
    const double sStep = std::max(0.02, (scaleMax - scaleMin) / 10.0);
    for (double s = scaleMin; s <= scaleMax + 1e-9; s += sStep) scales.push_back(s);

    // 生成指定角度/缩放的旋转画布模板 (原图分辨率)
    auto buildRotated = [&](double scale, double angle, cv::Mat& rotated, double& rotC, int& stw, int& sth) {
        cv::Mat scT;
        cv::resize(templ, scT, cv::Size(), scale, scale, cv::INTER_LINEAR);
        stw = scT.cols; sth = scT.rows;
        const double sDia = std::ceil(std::hypot(stw, sth));
        cv::Mat rot = cv::getRotationMatrix2D(cv::Point2f(stw / 2.0f, sth / 2.0f), angle, 1.0);
        cv::warpAffine(scT, rotated, rot, cv::Size((int)sDia, (int)sDia),
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));
        rotC = (sDia - stw) / 2.0;   // 画布上模板左上角偏移(近似)
        return (int)sDia;
    };

    // ============ 粗阶段: 1/4 分辨率全姿态搜索 ============
    const int F = 4;
    const int coarseMin = 48;   // 粗图短边下限
    int factor = F;
    while (factor > 1 &&
           (std::min(src.cols, src.rows) / factor < coarseMin ||
            std::min(tw, th) / factor < 8))
        factor /= 2;

    struct CoarseCand { double score; double cx; double cy; double angle; double scale; };
    std::vector<CoarseCand> coarseCands;
    const double coarseThreshold = threshold * 0.90;   // 粗阶段放宽, 防止漏掉最佳姿态

    cv::Mat srcC;
    cv::resize(src, srcC, cv::Size(), 1.0 / factor, 1.0 / factor, cv::INTER_AREA);
    cv::Mat result;
    for (double scale : scales) {
        for (double angle : angles) {
            cv::Mat rotated; double rotC; int stw, sth;
            const int sDia = buildRotated(scale, angle, rotated, rotC, stw, sth);
            cv::Mat rotC_s;
            if (factor > 1) {
                cv::resize(rotated, rotC_s, cv::Size(), 1.0 / factor, 1.0 / factor, cv::INTER_AREA);
            } else rotC_s = rotated;
            if (rotC_s.cols > srcC.cols || rotC_s.rows > srcC.rows) continue;
            cv::matchTemplate(srcC, rotC_s, result, cv::TM_CCOEFF_NORMED);
            double minVal, maxVal; cv::Point minLoc, maxLoc;
            cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
            if (maxVal < coarseThreshold) continue;
            // 粗图匹配点 → 原图模板中心坐标 (未做反旋细节修正, 粗定位足够)
            const double cxFull = (maxLoc.x + rotC + stw / 2.0) * factor;
            const double cyFull = (maxLoc.y + rotC + sth / 2.0) * factor;
            coarseCands.push_back({maxVal, cxFull, cyFull, angle, scale});
        }
    }

    // 候选去重 (位置邻近) 并取最优少数
    std::sort(coarseCands.begin(), coarseCands.end(),
              [](const CoarseCand& a, const CoarseCand& b) { return a.score > b.score; });
    std::vector<CoarseCand> seeds;
    for (const auto& c : coarseCands) {
        bool dup = false;
        for (const auto& s : seeds) {
            if (std::hypot(s.cx - c.cx, s.cy - c.cy) < dia / 2.0) { dup = true; break; }
        }
        if (!dup) seeds.push_back(c);
        if ((int)seeds.size() >= 5) break;
    }
    if (seeds.empty()) {
        setResultData("matchCount", 0);
        setResultData("found", false);
        setResultData("score", 0.0);
        setStatus(ToolStatus::NG);
        return false;
    }

    // ============ 细阶段: 原分辨率局部窗口精搜 ============
    for (const auto& seed : seeds) {
        // 角度/缩放局部细窗 (±2粗步长, 半步长)
        const double aWin = std::max(angleStep, 0.2) * 2.0;
        const double aFine = std::max(0.05, angleStep / 2.0);
        const double sWin = std::max(sStep, 0.02) * 2.0;
        const double sFine = std::max(0.005, sStep / 2.0);

        double bestScore = -1;
        Match best{seed.cx, seed.cy, seed.score, seed.angle, seed.scale};

        for (double scale = std::max(scaleMin, seed.scale - sWin);
             scale <= std::min(scaleMax, seed.scale + sWin) + 1e-9; scale += sFine) {
            for (double angle = std::max(angleStart, seed.angle - aWin);
                 angle <= std::min(angleEnd, seed.angle + aWin) + 1e-9; angle += aFine) {
                cv::Mat rotated; double rotC; int stw, sth;
                const int sDia = buildRotated(scale, angle, rotated, rotC, stw, sth);
                if (rotated.cols > src.cols || rotated.rows > src.rows) continue;

                // 局部窗口: 以种子位置为中心, 窗口=模板画布+边距
                const int margin = 4;
                int wx = (int)std::lround(seed.cx - sDia / 2.0) - margin;
                int wy = (int)std::lround(seed.cy - sDia / 2.0) - margin;
                int ww = sDia + margin * 2, wh = sDia + margin * 2;
                wx = std::max(0, wx); wy = std::max(0, wy);
                ww = std::min(ww, src.cols - wx);
                wh = std::min(wh, src.rows - wy);
                if (ww < rotated.cols || wh < rotated.rows) continue;

                const cv::Mat roi = src(cv::Rect(wx, wy, ww, wh));
                cv::matchTemplate(roi, rotated, result, cv::TM_CCOEFF_NORMED);
                double minVal, maxVal; cv::Point minLoc, maxLoc;
                cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
                if (maxVal > bestScore) {
                    bestScore = maxVal;
                    const double offX = wx + maxLoc.x + rotC + stw / 2.0;
                    const double offY = wy + maxLoc.y + rotC + sth / 2.0;
                    const double radA = angle * CV_PI / 180.0;
                    const double cc = std::cos(radA), ss = std::sin(radA);
                    const double cxT = stw / 2.0, cyT = sth / 2.0;
                    // 反旋转到未旋转模板坐标 (与原实现一致)
                    const double rx = cxT + (offX - wx - cxT) * cc + (offY - wy - cyT) * ss;
                    const double ry = cyT - (offX - wx - cxT) * ss + (offY - wy - cyT) * cc;
                    best = Match{rx + wx, ry + wy, maxVal, angle, scale};
                }
            }
        }

        if (bestScore >= threshold)
            matches.push_back(best);
        else if (seed.score >= threshold)
            matches.push_back(Match{seed.cx, seed.cy, seed.score, seed.angle, seed.scale});
    }

    std::sort(matches.begin(), matches.end(),
              [](const Match& a, const Match& b) { return a.score > b.score; });
    // 去重: 同一位置附近的重复候选仅保留最高分
    std::vector<Match> uniq;
    for (const auto& m : matches) {
        bool dup = false;
        for (const auto& u : uniq) {
            if (std::fabs(u.x - m.x) < templ.cols / 2.0 && std::fabs(u.y - m.y) < templ.rows / 2.0) { dup = true; break; }
        }
        if (!dup) uniq.push_back(m);
        if ((int)uniq.size() >= maxMatches) break;
    }

    setResultData("matchCount", (int)uniq.size());
    if (!uniq.empty()) {
        setResultData("matchX", uniq[0].x);
        setResultData("matchY", uniq[0].y);
        setResultData("score", uniq[0].score);
        setResultData("matchAngle", uniq[0].angle);
        setResultData("matchScale", uniq[0].scale);
        setResultData("found", true);
        setStatus(ToolStatus::OK);
    } else {
        setResultData("found", false);
        setResultData("score", 0.0);
        setStatus(ToolStatus::NG);
    }
    return !uniq.empty();
#else
    setResultData("error", "需要OpenCV库");
    setStatus(ToolStatus::NG);
    return false;
#endif
}

VI_REGISTER_TOOL(GrayscaleMatch, "灰度匹配", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector
