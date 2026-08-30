/**
 * @file ThreadInspection.cpp
 * @brief 螺纹检测专项工具实现 — 极坐标展开管线
 */
#include "ThreadInspection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <algorithm>
#include <cmath>
#include <QVariantMap>

namespace VisionInspector {

namespace {
constexpr double kPi = 3.14159265358979323846;

/** 一维信号局部峰值(考虑极性: darkTeeth时找谷) */
std::vector<int> findPeaks1D(const std::vector<double>& sig, double minProminence) {
    std::vector<int> peaks;
    const int n = (int)sig.size();
    for (int i = 0; i < n; ++i) {
        const double prev = sig[(i - 1 + n) % n];
        const double next = sig[(i + 1) % n];
        if (sig[i] >= prev && sig[i] >= next && sig[i] > minProminence)
            peaks.push_back(i);
    }
    // 合并相邻峰(环形距离1)
    std::vector<int> merged;
    for (int i = 0; i < (int)peaks.size(); ++i) {
        const int next = peaks[(i + 1) % peaks.size()];
        if ((next - peaks[i] + n) % n <= 1 && sig[next] > sig[i]) continue;
        merged.push_back(peaks[i]);
    }
    return merged;
}

/** 按最小间距合并峰: 幅值高的保留, 近邻剔除 */
std::vector<int> mergePeaks(const std::vector<double>& sig, std::vector<int> peaks, int minDist) {
    std::sort(peaks.begin(), peaks.end(), [&sig](int a, int b) {
        if (std::fabs(sig[a]) != std::fabs(sig[b]))
            return std::fabs(sig[a]) > std::fabs(sig[b]);
        return a < b;
    });
    std::vector<int> kept;
    const int n = (int)sig.size();
    for (int p : peaks) {
        bool tooClose = false;
        for (int k : kept) {
            int d = std::abs(k - p);
            d = std::min(d, n - d);
            if (d < minDist) { tooClose = true; break; }
        }
        if (!tooClose) kept.push_back(p);
    }
    std::sort(kept.begin(), kept.end());
    return kept;
}

double medianOf(std::vector<double> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

} // anonymous namespace

PropertyDefList ThreadInspection::propertyDefs() const {
    return {
        PropertyDef::doubleProp("roiCenterX", "工件中心X(-1=图像中心)", -1, -1, 10000, "定位"),
        PropertyDef::doubleProp("roiCenterY", "工件中心Y(-1=图像中心)", -1, -1, 10000, "定位"),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "定位"),
        PropertyDef::doubleProp("partRadius", "工件外径半径", 110, 1, 5000, "定位"),
        PropertyDef::enumProp("threadPolarity", "牙型极性", {"暗牙(亮底)", "亮牙(暗底)"}, 0, "检测"),
        PropertyDef::doubleProp("threadInnerRatio", "螺纹环内径占比", 0.72, 0.1, 0.98, "检测"),
        PropertyDef::doubleProp("threadOuterRatio", "螺纹环外径占比", 1.0, 0.3, 1.0, "检测"),
        PropertyDef::intProp("expectedTeeth", "期望牙数(0=自动)", 0, 0, 360, "检测"),
        PropertyDef::doubleProp("gapRatio", "缺牙间距倍数", 1.45, 1.1, 3.0, "检测"),
        PropertyDef::doubleProp("damageRatio", "烂牙幅值比", 0.55, 0.2, 0.95, "检测"),
        PropertyDef::boolProp("checkBurr", "毛刺检测(外缘带)", true, "检测"),
        PropertyDef::doubleProp("burrBandRatio", "毛刺带宽占比", 0.10, 0.02, 0.3, "检测"),
        PropertyDef::intProp("angularSamples", "展开角向采样数", 1440, 360, 4096, "高级"),
    };
}

bool ThreadInspection::execute(ToolContext& context) {
#ifndef VI_HAS_OPENCV
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#else
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        setResultData("error", "输入图像为空"); setStatus(ToolStatus::NG); return false;
    }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    // 中心: 属性<0表示用图像中心 (配合引用 "$(快速找圆.centerX)" 链式定位)
    double cx = propertyValue("roiCenterX").toDouble();
    double cy = propertyValue("roiCenterY").toDouble();
    if (cx < 0) cx = src.cols / 2.0;
    if (cy < 0) cy = src.rows / 2.0;
    {
        double ang = 0;   // 螺纹检测无角度
        applyCorrection(context, cx, cy, ang);   // 位置补正跟随
    }
    const double partR = propertyValue("partRadius").toDouble();
    m_center = cv::Point2d(cx, cy);
    m_outerR = partR;

    const bool darkTeeth = propertyValue("threadPolarity").toInt() == 0;
    const double innerRatio = propertyValue("threadInnerRatio").toDouble();
    const double outerRatio = propertyValue("threadOuterRatio").toDouble();
    const int expectedTeeth = propertyValue("expectedTeeth").toInt();
    const double gapRatio = propertyValue("gapRatio").toDouble();
    const double damageRatio = propertyValue("damageRatio").toDouble();
    const bool checkBurr = propertyValue("checkBurr").toBool();
    const double burrBand = propertyValue("burrBandRatio").toDouble();
    const int samples = propertyValue("angularSamples").toInt();

    // ---- 1. 极坐标展开 ----
    // 注意: 实测本OpenCV 5.0.0的warpPolar输出 x列=半径(0..maxR), y行=角度(0..2π)
    const double maxR = partR * (1.0 + (checkBurr ? burrBand : 0.02)) + 6;
    cv::Mat polar;
    const int radialSamples = std::max(128, (int)(maxR * 1.5));
    cv::warpPolar(src, polar, cv::Size(radialSamples, samples),
                  cv::Point2f((float)cx, (float)cy), (float)maxR,
                  cv::WARP_POLAR_LINEAR);
    // polar: 列=半径(0..maxR), 行=角度(0..2π)

    // 半径列映射: col r_px = r / maxR * radialSamples
    const int colInner = std::max(0, (int)(partR * innerRatio / maxR * radialSamples));
    const int colOuter = std::min(radialSamples - 1, (int)(partR * outerRatio / maxR * radialSamples));
    if (colOuter - colInner < 4) {
        setResultData("error", "螺纹环带太窄, 检查内外径占比");
        setStatus(ToolStatus::NG);
        return false;
    }

    // ---- 2. 角度投影 (环带均值, 沿半径方向平均 -> 每个角度一个值) ----
    std::vector<double> proj(samples, 0.0);
    for (int a = 0; a < samples; ++a) {
        const uchar* row = polar.ptr<uchar>(a);
        for (int c = colInner; c <= colOuter; ++c)
            proj[a] += row[c];
    }
    const double bandN = colOuter - colInner + 1;
    for (int a = 0; a < samples; ++a) proj[a] /= bandN;

    // ---- 3. 峰值检测 (暗牙找谷 → 取反) ----
    if (darkTeeth)
        for (auto& v : proj) v = -v;
    // 去基线
    double baseline = medianOf(proj);
    for (auto& v : proj) v -= baseline;
    double maxAmp = *std::max_element(proj.begin(), proj.end());

    // 角度平滑: 抑制牙内条纹振铃 (窗口≈1/4牙距)
    const int toothStride = (expectedTeeth > 0) ? samples / expectedTeeth : samples / 60;
    const int smoothW = std::max(2, toothStride / 12);   // 窗口须远小于牙宽(约1/5牙距)
    {
        std::vector<double> sm(samples, 0.0);
        for (int i = 0; i < samples; ++i) {
            double sum = 0; int cnt = 0;
            for (int k = -smoothW; k <= smoothW; ++k) {
                int j = (i + k + samples) % samples;
                sum += proj[j]; ++cnt;
            }
            sm[i] = sum / cnt;
        }
        proj = sm;
        maxAmp = *std::max_element(proj.begin(), proj.end());
    }

    std::vector<int> peaks = findPeaks1D(proj, maxAmp * 0.25);
    // 按最小牙距合并 (半牙距内只留最强峰)
    peaks = mergePeaks(proj, peaks, std::max(2, toothStride / 2));
    const int toothCount = (int)peaks.size();


    // ---- 4. 牙距 ----
    std::vector<double> gaps;
    for (size_t i = 0; i < peaks.size(); ++i) {
        const size_t j = (i + 1) % peaks.size();
        double gap = (double)peaks[j] - peaks[i];
        if (j == 0) gap += samples;   // 环形回绕
        gaps.push_back(gap);
    }
    const double medGap = medianOf(gaps);
    const double pitchDeg = medGap * 360.0 / samples;
    const double pitchPx = pitchDeg / 360.0 * 2.0 * kPi * partR;

    // ---- 5. 缺牙检测 ----
    std::vector<double> missingAnglesDeg;
    if (expectedTeeth > 0 || !gaps.empty()) {
        const double refGap = (expectedTeeth > 0) ? (double)samples / expectedTeeth : medGap;
        for (size_t i = 0; i < gaps.size(); ++i) {
            if (gaps[i] > refGap * gapRatio) {
                // 缺口中心角度
                int start = peaks[i];
                int span = (int)gaps[i];
                const double midCol = std::fmod((double)start + span / 2.0, samples);
                missingAnglesDeg.push_back(midCol * 360.0 / samples);
            }
        }
    }

    // ---- 6. 烂牙检测 (幅值坍塌的峰) ----
    std::vector<double> amps;
    for (int p : peaks) amps.push_back(proj[p]);
    const double medAmp = medianOf(amps);
    std::vector<double> damageAnglesDeg;
    for (int p : peaks)
        if (proj[p] < medAmp * damageRatio)
            damageAnglesDeg.push_back(p * 360.0 / samples);

    // ---- 7. 毛刺检测 (外缘带亮点) ----
    std::vector<cv::Point2d> burrPoints;
    if (checkBurr) {
        const int colBurr0 = std::min(radialSamples - 2, (int)(partR * 1.0 / maxR * radialSamples) + 2);
        const int colBurr1 = radialSamples - 1;
        if (colBurr1 > colBurr0) {
            std::vector<double> outerProj(samples, 0.0);
            for (int a = 0; a < samples; ++a) {
                const uchar* row = polar.ptr<uchar>(a);
                for (int c = colBurr0; c <= colBurr1; ++c)
                    outerProj[a] = std::max(outerProj[a], (double)row[c]);
            }
            // 背景亮度估计(带内中位)
            std::vector<double> sorted = outerProj;
            std::sort(sorted.begin(), sorted.end());
            const double bg = sorted[samples / 2];
            for (int a = 0; a < samples; ++a) {
                if (outerProj[a] > bg + 60) {
                    const double angDeg = a * 360.0 / samples;
                    const double rr = partR * 1.03;
                    burrPoints.push_back(cv::Point2d(cx + std::cos(angDeg * kPi / 180) * rr,
                                                     cy + std::sin(angDeg * kPi / 180) * rr));
                    a += samples / 90;   // 跳过相邻列(同一毛刺)
                }
            }
        }
    }

    // ---- 8. 斜牙检测 (内外半带峰值相位差) ----
    double slantAngle = 0;
    {
        const int colMid = (colInner + colOuter) / 2;
        std::vector<double> projIn(samples, 0.0), projOut(samples, 0.0);
        for (int a = 0; a < samples; ++a) {
            const uchar* row = polar.ptr<uchar>(a);
            for (int c = colInner; c <= colMid; ++c) projIn[a] += row[c];
            for (int c = colMid + 1; c <= colOuter; ++c) projOut[a] += row[c];
        }
        const double nIn = colMid - colInner + 1.0, nOut = colOuter - colMid + 0.0;
        for (int a = 0; a < samples; ++a) {
            if (darkTeeth) { projIn[a] = -projIn[a] / nIn; projOut[a] = -projOut[a] / nOut; }
            else { projIn[a] /= nIn; projOut[a] /= nOut; }
        }
        for (auto* p : {&projIn, &projOut}) {
            double base = 0;   // 去基线
            std::vector<double> tmp = *p;
            std::sort(tmp.begin(), tmp.end());
            base = tmp[samples / 2];
            for (auto& v : *p) v -= base;
        }
        std::vector<int> in = mergePeaks(projIn, findPeaks1D(projIn, maxAmp * 0.25),
                                         std::max(2, toothStride / 2));
        std::vector<int> out = mergePeaks(projOut, findPeaks1D(projOut, maxAmp * 0.25),
                                          std::max(2, toothStride / 2));
        if (!in.empty() && !out.empty()) {
            // 每个内圈峰找最近的外圈峰, 相位差取中位
            std::vector<double> shifts;
            for (int pi : in) {
                int best = out[0];
                int bestD = samples;
                for (int po : out) {
                    int d = std::abs(po - pi);
                    d = std::min(d, samples - d);
                    if (d < bestD) { bestD = d; best = po; }
                }
                double sh = (double)(best - pi);
                if (sh > samples / 2.0) sh -= samples;
                if (sh < -samples / 2.0) sh += samples;
                shifts.push_back(sh * 360.0 / samples);
            }
            slantAngle = medianOf(shifts);
        }
    }

    // ---- 9. 牙外径 (外缘带投影的最大半径处) ----
    double outerRadius = partR;
    {
        // 用外缘带第一个强亮行估计: 简化v1返回名义外径
        outerRadius = partR;
    }

    // ---- 结果 ----
    m_lastOk = true;
    m_toothAngles.clear();
    for (int p : peaks) m_toothAngles.push_back(p * 360.0 / samples);
    m_missingAngles = missingAnglesDeg;
    m_burrPoints = burrPoints;
    m_damageAngles = damageAnglesDeg;

    setResultData("toothCount", toothCount);
    setResultData("expectedTeeth", expectedTeeth);
    setResultData("pitchDeg", pitchDeg);
    setResultData("pitchPx", pitchPx);
    setResultData("missingCount", (int)missingAnglesDeg.size());
    setResultData("damageCount", (int)damageAnglesDeg.size());
    setResultData("burrCount", (int)burrPoints.size());
    setResultData("slantAngle", slantAngle);
    setResultData("outerDiameterPx", outerRadius * 2.0);
    setResultData("found", toothCount > 0);

    const bool ok = toothCount > 0
                    && (expectedTeeth <= 0 || std::abs(toothCount - expectedTeeth) <= 1)
                    && missingAnglesDeg.empty();
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
#endif
}

std::vector<QVariant> ThreadInspection::overlays() const {
    std::vector<QVariant> out;
#ifndef VI_HAS_OPENCV
    return out;
#else
    if (!m_lastOk) return out;
    const double rIn = m_outerR * 0.72, rOut = m_outerR;
    // 检出的牙 (绿色径向刻线)
    QVariantList goodLines;
    for (double a : m_toothAngles) {
        const double rad = a * kPi / 180.0;
        goodLines.append(QVariantList{
            m_center.x + std::cos(rad) * rIn, m_center.y + std::sin(rad) * rIn,
            m_center.x + std::cos(rad) * rOut, m_center.y + std::sin(rad) * rOut});
    }
    QVariantMap good; good["type"] = "lines"; good["lines"] = goodLines;
    good["color"] = "#00ff00"; out.push_back(good);
    // 缺牙 (红色扇区标记)
    QVariantList missLines;
    for (double a : m_missingAngles) {
        const double rad = a * kPi / 180.0;
        for (double rr : {rIn, (rIn + rOut) / 2, rOut}) {
            missLines.append(QVariantList{
                m_center.x + std::cos(rad) * (rr - 6), m_center.y + std::sin(rad) * (rr - 6),
                m_center.x + std::cos(rad) * (rr + 6), m_center.y + std::sin(rad) * (rr + 6)});
        }
    }
    if (!missLines.isEmpty()) {
        QVariantMap miss; miss["type"] = "lines"; miss["lines"] = missLines;
        miss["color"] = "#ff2020"; out.push_back(miss);
    }
    // 烂牙 (橙色刻线)
    QVariantList dmgLines;
    for (double a : m_damageAngles) {
        const double rad = a * kPi / 180.0;
        dmgLines.append(QVariantList{
            m_center.x + std::cos(rad) * rIn, m_center.y + std::sin(rad) * rIn,
            m_center.x + std::cos(rad) * rOut, m_center.y + std::sin(rad) * rOut});
    }
    if (!dmgLines.isEmpty()) {
        QVariantMap dmg; dmg["type"] = "lines"; dmg["lines"] = dmgLines;
        dmg["color"] = "#ff9900"; out.push_back(dmg);
    }
    // 毛刺 (红点)
    if (!m_burrPoints.empty()) {
        QVariantMap burr; burr["type"] = "points";
        QVariantList bp;
        for (const auto& p : m_burrPoints) bp.append(QVariantList{p.x, p.y});
        burr["pts"] = bp; burr["color"] = "#ff2020";
        out.push_back(burr);
    }
    // 外径圆
    QVariantMap circle;
    circle["type"] = "circle";
    circle["cx"] = m_center.x; circle["cy"] = m_center.y; circle["r"] = m_outerR;
    circle["color"] = "#00aaff";
    out.push_back(circle);
    // 结果文本
    QVariantMap text;
    text["type"] = "text";
    text["x"] = 10.0; text["y"] = 20.0; text["size"] = 14.0;
    text["text"] = QString("牙数:%1 缺牙:%2 烂牙:%3 毛刺:%4 斜牙:%5°")
                       .arg(m_toothAngles.size()).arg(m_missingAngles.size())
                       .arg(m_damageAngles.size()).arg(m_burrPoints.size())
                       .arg(0.0, 0, 'f', 1);
    text["color"] = "#ffff00";
    out.push_back(text);
    return out;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ThreadInspection, "螺纹检测", VisionInspector::ToolCategory::Special)
