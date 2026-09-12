/** @file FitLine.cpp - 拟合直线工具实现
 *
 *  点集来源 (自动探测, 按优先级):
 *    1. ScanEdge 输出: edgeX[] + edgeY[] 列表
 *    2. 任意上游: points / pts / edgePoints 键 (QVariantList of [x,y])
 *  拟合: SubpixEdge::fitLineRobust (质心+PCA+外点剔除)
 */
#include "FitLine.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <cmath>
#endif

namespace VisionInspector {

PropertyDefList FitLine::propertyDefs() const {
    return {
        PropertyDef::stringProp("sourceTool", "点集来源工具(留空=自动向前找)", "", "来源"),
        PropertyDef::doubleProp("extendLen", "绘制延长(px)", 100, 0, 5000, "显示"),
    };
}

bool FitLine::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    m_lastOk = false;
    // ---- 收集点集 ----
    std::vector<cv::Point2d> pts;

    // 来源1: ScanEdge 的 edgeX/edgeY 双列表 (自动: 找 flow 里最近 ScanEdge)
    // 简化: 直接扫描上下文键名, 匹配 *.edgeX 存在即用
    const DataMap all = context.allData();
    QString edgeKey;
    for (auto it = all.begin(); it != all.end(); ++it) {
        if (it.key().endsWith(QStringLiteral(".edgeX"))) { edgeKey = it.key(); break; }
    }
    if (!edgeKey.isEmpty()) {
        QString yKey = edgeKey;
        yKey.replace(QStringLiteral(".edgeX"), QStringLiteral(".edgeY"));
        const QVariantList xs = all.value(edgeKey).toList();
        const QVariantList ys = all.value(yKey).toList();
        const int n = qMin(xs.size(), ys.size());
        for (int i = 0; i < n; ++i)
            pts.push_back({ xs[i].toDouble(), ys[i].toDouble() });
        setResultData("srcKey", edgeKey);
        setResultData("srcCount", n);
    }

    // 来源2: points/pts 通用键
    if (pts.empty()) {
        for (const char* k : { "points", "pts", "edgePoints" }) {
            if (!context.hasData(k)) continue;
            const QVariantList arr = context.getData(k).toList();
            for (const QVariant& pv : arr) {
                const QVariantList xy = pv.toList();
                if (xy.size() >= 2) pts.push_back({ xy[0].toDouble(), xy[1].toDouble() });
            }
            if (!pts.empty()) break;
        }
    }

    if (pts.size() < 3) {
        setResultData("error", QStringLiteral("点集不足(%1个, 需>=3) — 来源: ScanEdge的edgeX/Y或points键").arg(pts.size()));
        setStatus(ToolStatus::NG);
        return false;
    }

    // ---- 鲁棒拟合 ----
    cv::Point2d dir, origin; double rms = 0;
    if (!fitLineRobust(pts, dir, origin, rms)) {
        setResultData("error", "直线拟合退化 (点共线判定失败)");
        setStatus(ToolStatus::NG);
        return false;
    }

    // 输出 (与检测直线同约定: 中心 + 角度)
    double angleDeg = std::atan2(dir.y, dir.x) * 180.0 / CV_PI;
    // 规范到 (-90, 90]
    if (angleDeg > 90) angleDeg -= 180;
    if (angleDeg <= -90) angleDeg += 180;

    const double ext = propertyValue("extendLen").toDouble();
    m_x = origin.x; m_y = origin.y; m_angle = angleDeg;
    m_x1 = origin.x - dir.x * ext; m_y1 = origin.y - dir.y * ext;
    m_x2 = origin.x + dir.x * ext; m_y2 = origin.y + dir.y * ext;

    setResultData("centerX", origin.x);
    setResultData("centerY", origin.y);
    setResultData("angle", angleDeg);
    setResultData("rms", rms);
    setResultData("pointCount", (int)pts.size());
    setResultData("dirX", dir.x);
    setResultData("dirY", dir.y);
    setResultData("x1", m_x1); setResultData("y1", m_y1);
    setResultData("x2", m_x2); setResultData("y2", m_y2);

    m_lastOk = true;
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

std::vector<QVariant> FitLine::overlays() const {
    std::vector<QVariant> out;
    if (!m_lastOk) return out;
    QVariantMap line;
    line["type"] = "line";
    line["x1"] = m_x1; line["y1"] = m_y1;
    line["x2"] = m_x2; line["y2"] = m_y2;
    line["color"] = "#00c8ff";
    out.push_back(line);
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(FitLine, "拟合直线", VisionInspector::ToolCategory::Geometry)
