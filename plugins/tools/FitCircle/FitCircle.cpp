/** @file FitCircle.cpp - 拟合圆工具实现 */
#include "FitCircle.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <cmath>
#endif

namespace VisionInspector {

PropertyDefList FitCircle::propertyDefs() const {
    return {
        PropertyDef::stringProp("sourceTool", "点集来源工具(留空=自动找edgeX/Y)", "", "来源"),
    };
}

bool FitCircle::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    m_lastOk = false;
    std::vector<cv::Point2d> pts;

    const DataMap all = context.allData();
    // 来源工具指定 → 精确键 "来源.edgeX"; 留空 → 第一个 .edgeX 结尾键
    const QString want = propertyValue("sourceTool").toString().trimmed();
    QString edgeKey;
    if (!want.isEmpty()) {
        const QString k = want + QStringLiteral(".edgeX");
        if (context.hasData(k)) edgeKey = k;
    }
    if (edgeKey.isEmpty()) {
        for (auto it = all.begin(); it != all.end(); ++it) {
            if (it.key().endsWith(QStringLiteral(".edgeX"))) { edgeKey = it.key(); break; }
        }
    }
    if (!edgeKey.isEmpty()) {
        QString yKey = edgeKey;
        yKey.replace(QStringLiteral(".edgeX"), QStringLiteral(".edgeY"));
        const QVariantList xs = all.value(edgeKey).toList();
        const QVariantList ys = all.value(yKey).toList();
        const int n = qMin(xs.size(), ys.size());
        for (int i = 0; i < n; ++i)
            pts.push_back({ xs[i].toDouble(), ys[i].toDouble() });
    }
    if (pts.empty()) {
        for (const char* k : { "points", "pts", "contourPoints", "edgePoints" }) {
            if (!context.hasData(k)) continue;
            const QVariantList arr = context.getData(k).toList();
            for (const QVariant& pv : arr) {
                const QVariantList xy = pv.toList();
                if (xy.size() >= 2) pts.push_back({ xy[0].toDouble(), xy[1].toDouble() });
            }
            if (!pts.empty()) break;
        }
    }

    if (pts.size() < 5) {
        setResultData("error", QStringLiteral("点集不足(%1个, 需>=5) — 来源: edgeX/Y或points键").arg(pts.size()));
        setStatus(ToolStatus::NG);
        return false;
    }

    cv::Point2d center; double radius = 0, rms = 0;
    if (!fitCircleRobust(pts, center, radius, rms)) {
        setResultData("error", "圆拟合退化");
        setStatus(ToolStatus::NG);
        return false;
    }

    m_cx = center.x; m_cy = center.y; m_r = radius;
    m_lastOk = true;

    setResultData("centerX", center.x);
    setResultData("centerY", center.y);
    setResultData("radius", radius);
    setResultData("rms", rms);
    setResultData("pointCount", (int)pts.size());

    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

std::vector<QVariant> FitCircle::overlays() const {
    std::vector<QVariant> out;
    if (!m_lastOk) return out;
    QVariantMap circle;
    circle["type"] = "circle";
    circle["cx"] = m_cx; circle["cy"] = m_cy; circle["r"] = m_r;
    circle["color"] = "#00c8ff";
    out.push_back(circle);
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(FitCircle, "拟合圆", VisionInspector::ToolCategory::Geometry)
