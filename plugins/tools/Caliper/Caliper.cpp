/**
 * @file Caliper.cpp
 * @brief 卡尺测量工具 — 亚像素版
 *
 * 算法 (阶段2升级 + #4 边缘对模式):
 *   1. 旋转卡尺: 沿ROI角度方向的扫描线(长度=roiWidth, 宽度=scanWidth平均)
 *   2. 双线性剖面采样 + 梯度峰值抛物线内插 → 亚像素边缘位置
 *   3. 边缘对模式: 找剖面上全部边缘, 按"边缘选择"输出
 *      - 第一边缘: 扫描起点到第一条边缘的距离 (原行为, 兼容)
 *      - 第一对/最强对: 两条边缘之间的真实间距 (对齐CKVision卡尺)
 *      - 全部边缘: 输出全部边缘位置与相邻间距
 * 输出: edgePositionX/Y(图像坐标), width(边缘间距), strength, found, edgeCount
 */
#include "Caliper.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <cmath>
#include <QVariantMap>
#include <QVariantList>

namespace VisionInspector {

PropertyDefList Caliper::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 1),
        PropertyDef::doubleProp("roiCenterX", "卡尺中心X", 320, 0, 10000, "卡尺"),
        PropertyDef::doubleProp("roiCenterY", "卡尺中心Y", 240, 0, 10000, "卡尺"),
        PropertyDef::doubleProp("roiWidth", "卡尺长度(扫描距离)", 120, 1, 10000, "卡尺"),
        PropertyDef::doubleProp("roiHeight", "卡尺宽度(平均高)", 10, 1, 1000, "卡尺"),
        PropertyDef::doubleProp("roiAngle", "卡尺角度", 0, -180, 180, "卡尺"),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "卡尺"),
        PropertyDef::enumProp("edgeMode", "边缘模式",
                              {"第一边缘", "第一对边缘", "最强对边缘", "全部边缘"}, 1, "检测"),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0, "检测"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 30, 1, 255, "检测"),
        PropertyDef::intProp("filterHalfWidth", "梯度平滑半宽", 2, 1, 20, "检测"),
        PropertyDef::intProp("scanWidth", "扫描宽度", 1, 1, 100, "检测"),
    };
}

bool Caliper::execute(ToolContext& context) {
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

    double cx = propertyValue("roiCenterX").toDouble();
    double cy = propertyValue("roiCenterY").toDouble();
    const double roiW = propertyValue("roiWidth").toDouble();
    const double roiH = propertyValue("roiHeight").toDouble();
    double roiAng = propertyValue("roiAngle").toDouble();
    applyCorrection(context, cx, cy, roiAng);   // 位置补正跟随

    ScanOptions opt;
    opt.polarity = propertyValue("edgePolarity").toInt();
    opt.gradThreshold = propertyValue("gradientThreshold").toInt();
    opt.filterHalfWidth = propertyValue("filterHalfWidth").toInt();

    // 扫描线: 沿卡尺角度方向, 从起点到终点
    const double rad = roiAng * CV_PI / 180.0;
    const cv::Point2d u(std::cos(rad), std::sin(rad));
    m_scanStart = cv::Point2d(cx - u.x * roiW / 2, cy - u.y * roiW / 2);
    m_scanEnd   = cv::Point2d(cx + u.x * roiW / 2, cy + u.y * roiW / 2);

    // 全部边缘 (按扫描顺序); scanWidth>1 时在垂直方向取多条平行线平均剖面 (降噪)
    const int scanW = propertyValue("scanWidth").toInt();
    std::vector<SubpixEdgePoint> edges;
    if (scanW <= 1) {
        edges = findEdgesSubpix(src, m_scanStart, m_scanEnd, opt, 0);
    } else {
        const cv::Point2d n(-u.y, u.x);
        std::vector<std::vector<double>> profiles;
        int steps = 0;
        for (int k = 0; k < scanW; ++k) {
            const double off = (k - (scanW - 1) / 2.0);
            const cv::Point2d s0 = m_scanStart + n * off;
            const cv::Point2d s1 = m_scanEnd + n * off;
            if (k == 0) steps = std::max(4, (int)std::lround(std::hypot(s1.x - s0.x, s1.y - s0.y)));
            // 采样每条线的剖面
            std::vector<double> prof(steps + 1, 0.0);
            for (int i2 = 0; i2 <= steps; ++i2) {
                const double t = (double)i2 / steps;
                const cv::Point2d p = s0 + (s1 - s0) * t;
                prof[i2] = sampleBilinear(src, p.x, p.y);
            }
            profiles.push_back(std::move(prof));
        }
        // 平均剖面 → 合成灰度线再走标准亚像素搜索
        cv::Mat lineImg(1, steps + 1, CV_8UC1);
        for (int i2 = 0; i2 <= steps; ++i2) {
            double sum = 0;
            for (const auto& prof : profiles) sum += prof[i2];
            lineImg.at<uchar>(0, i2) = (uchar)std::clamp(sum / profiles.size(), 0.0, 255.0);
        }
        edges = findEdgesSubpix(lineImg, cv::Point2d(0, 0), cv::Point2d((double)steps, 0), opt, 0);
        // 坐标映射回原图: 沿 u 从 m_scanStart
        for (auto& e : edges) {
            const double t = e.pos.x / steps;
            e.pos = m_scanStart + (m_scanEnd - m_scanStart) * t;
        }
        // 平均剖面输出 (曲线页数据源)
        QVariantList prof;
        for (int i2 = 0; i2 <= steps; ++i2)
            prof.append(QVariantList{ (double)i2, lineImg.at<uchar>(0, i2) });
        setResultData("profile", prof);
    }
    // 单线模式剖面
    if (scanW <= 1) {
        QVariantList prof;
        const int steps2 = std::max(4, (int)std::lround(std::hypot(
            m_scanEnd.x - m_scanStart.x, m_scanEnd.y - m_scanStart.y)));
        for (int i2 = 0; i2 <= steps2; ++i2) {
            const double t = (double)i2 / steps2;
            const cv::Point2d p = m_scanStart + (m_scanEnd - m_scanStart) * t;
            prof.append(QVariantList{ (double)i2, sampleBilinear(src, p.x, p.y) });
        }
        setResultData("profile", prof);
    }
    m_edges.clear();
    for (const auto& e : edges) m_edges.push_back(e.pos);

    const int edgeMode = propertyValue("edgeMode").toInt();
    setResultData("edgeCount", (int)edges.size());

    if (edges.empty()) {
        setResultData("found", false);
        setResultData("error", "扫描方向未找到超阈值边缘");
        setStatus(ToolStatus::NG);
        return false;
    }

    // 距离: 边缘沿扫描方向距起点的距离
    auto distFromStart = [&](const SubpixEdgePoint& e) {
        return std::hypot(e.pos.x - m_scanStart.x, e.pos.y - m_scanStart.y);
    };

    // ---- 全部边缘模式: 输出列表 + 相邻间距 ----
    if (edgeMode == 3) {
        QVariantList xs, ys, strengths, gaps;
        double prev = -1;
        for (const auto& e : edges) {
            const double d = distFromStart(e);
            xs.append(e.pos.x); ys.append(e.pos.y); strengths.append(e.strength);
            if (prev >= 0) gaps.append(d - prev);
            prev = d;
        }
        setResultData("edgeXs", xs);
        setResultData("edgeYs", ys);
        setResultData("edgeStrengths", strengths);
        setResultData("edgeGaps", gaps);
        // width = 首末边缘总跨度; 主边缘 = 最强
        size_t best = 0;
        for (size_t i = 1; i < edges.size(); ++i)
            if (edges[i].strength > edges[best].strength) best = i;
        m_edge = edges[best].pos;
        setResultData("edgePositionX", edges[best].pos.x);
        setResultData("edgePositionY", edges[best].pos.y);
        setResultData("width", distFromStart(edges.back()) - distFromStart(edges.front()));
        setResultData("strength", edges[best].strength);
        setResultData("found", true);
        setStatus(ToolStatus::OK);
        return true;
    }

    // ---- 单边缘模式 (原行为): 第一条边缘, width=起点到边缘距离 ----
    if (edgeMode == 0) {
        const SubpixEdgePoint& e = edges.front();
        m_edge = e.pos;
        setResultData("edgePositionX", e.pos.x);
        setResultData("edgePositionY", e.pos.y);
        setResultData("width", distFromStart(e));
        setResultData("strength", e.strength);
        setResultData("found", true);
        setStatus(ToolStatus::OK);
        return true;
    }

    // ---- 边缘对模式: 至少2条边缘 ----
    if (edges.size() < 2) {
        // 只有1条边缘: 退化为单边缘语义 (保持结果可用)
        const SubpixEdgePoint& e = edges.front();
        m_edge = e.pos;
        setResultData("edgePositionX", e.pos.x);
        setResultData("edgePositionY", e.pos.y);
        setResultData("width", distFromStart(e));
        setResultData("strength", e.strength);
        setResultData("found", true);
        setResultData("error", "仅找到1条边缘, 无边缘对");
        setStatus(ToolStatus::OK);
        return true;
    }

    // 第一对: 第1、2条边缘 / 最强对: 以最强边缘为中心取其相邻更近一侧组成对?
    // CKVision语义: 最强对=所有相邻对中"两条都强"的组合 — 简化为:
    //   第一对 = edges[0], edges[1]
    //   最强对 = 强度最大的两条相邻边缘 (按强度和最大)
    size_t i1 = 0, i2 = 1;
    if (edgeMode == 2) {
        double bestSum = -1;
        for (size_t i = 0; i + 1 < edges.size(); ++i) {
            const double s = edges[i].strength + edges[i + 1].strength;
            if (s > bestSum) { bestSum = s; i1 = i; i2 = i + 1; }
        }
    }
    const SubpixEdgePoint& e1 = edges[i1];
    const SubpixEdgePoint& e2 = edges[i2];
    const double gap = distFromStart(e2) - distFromStart(e1);

    m_edge = (e1.strength >= e2.strength) ? e1.pos : e2.pos;
    setResultData("edgePositionX", m_edge.x);
    setResultData("edgePositionY", m_edge.y);
    setResultData("edge1X", e1.pos.x);
    setResultData("edge1Y", e1.pos.y);
    setResultData("edge2X", e2.pos.x);
    setResultData("edge2Y", e2.pos.y);
    setResultData("width", gap);
    setResultData("strength", std::max(e1.strength, e2.strength));
    setResultData("found", true);
    setStatus(ToolStatus::OK);
    return true;
#endif
}

std::vector<QVariant> Caliper::overlays() const {
    std::vector<QVariant> out;
#ifdef VI_HAS_OPENCV
    // 卡尺框(青色)
    QVariantMap box;
    box["type"] = "line";
    box["x1"] = m_scanStart.x; box["y1"] = m_scanStart.y;
    box["x2"] = m_scanEnd.x;   box["y2"] = m_scanEnd.y;
    box["color"] = "#00aaff";
    out.push_back(box);
    // 全部边缘点(黄色)
    for (const auto& p : m_edges) {
        QVariantMap cross;
        cross["type"] = "cross";
        cross["cx"] = p.x;
        cross["cy"] = p.y;
        cross["size"] = 5.0;
        cross["color"] = "#ffff00";
        out.push_back(cross);
    }
#endif
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(Caliper, "卡尺测量", VisionInspector::ToolCategory::Geometry)
