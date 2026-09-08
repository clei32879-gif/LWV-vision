/** @file EdgeDepression.cpp - 边缘凹陷工具实现
 *
 *  算法管线 (复用 SubpixEdge 亚像素基础设施):
 *    1. 沿扫描角度布置 N 条平行扫描线, 每条取"起始边缘"(首个超阈值边缘) → 边缘点集
 *    2. 鲁棒直线拟合 (fitLineRobust, 外点自动剔除) → 基线
 *    3. 各点到基线有符号距离 (偏离为正) → 连续超阈值段聚合为凹陷/凸起段
 *    4. 段深/段宽判定 + 段数判定
 */
#include "EdgeDepression.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <QtGlobal>
#include <cmath>
#endif

namespace VisionInspector {

PropertyDefList EdgeDepression::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 0, "ROI"),
        PropertyDef::doubleProp("roiCenterX", "扫描中心X", 320, 0, 10000, "ROI"),
        PropertyDef::doubleProp("roiCenterY", "扫描中心Y", 240, 0, 10000, "ROI"),
        PropertyDef::doubleProp("roiWidth", "扫描宽度", 200, 1, 10000, "ROI"),
        PropertyDef::doubleProp("roiHeight", "扫描长度", 100, 1, 10000, "ROI"),
        PropertyDef::doubleProp("roiAngle", "扫描角度(度)", 0, -180, 180, "ROI"),

        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0, "边缘"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 40, 1, 255, "边缘"),
        PropertyDef::intProp("filterHalfWidth", "滤波半宽", 2, 0, 50, "边缘"),
        PropertyDef::enumProp("depressionDir", "检测方向",
            {"凹陷+凸起", "仅凹陷", "仅凸起"}, 0, "边缘"),

        PropertyDef::doubleProp("depressionThreshold", "凹陷深度阈值(px)", 5, 0.1, 1000, "判定"),
        PropertyDef::doubleProp("minWidth", "最小段宽(px)", 3, 0.5, 1000, "判定"),
        PropertyDef::intProp("minCount", "最少异常段数(0=不限)", 0, 0, 999, "判定"),
        PropertyDef::intProp("maxCount", "最多异常段数(0=不限)", 0, 0, 999, "判定"),
    };
}

bool EdgeDepression::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    m_lastOk = false;
    m_lastSegs.clear();

    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        setResultData("error", "输入图像为空"); setStatus(ToolStatus::NG); return false;
    }
    cv::Mat gray;
    if (input->channels() > 1) cv::cvtColor(*input, gray, cv::COLOR_BGR2GRAY);
    else gray = input->clone();

    // 诊断: 图像实际尺寸 (测试对照用)
    setResultData("imgW", gray.cols);
    setResultData("imgH", gray.rows);

    // 扫描几何 (与 ScanEdge 同约定): roiWidth=扫描线长度(沿扫描方向),
    // roiHeight=平行线总跨度(垂直方向); N 条平行线, 每条取"起始边缘"
    const double cx = propertyValue("roiCenterX").toDouble();
    const double cy = propertyValue("roiCenterY").toDouble();
    const double lineLen = propertyValue("roiWidth").toDouble();   // 单条扫描线长度
    const double span = propertyValue("roiHeight").toDouble();     // 平行线总跨度
    const double angleDeg = propertyValue("roiAngle").toDouble();
    const double a = angleDeg * CV_PI / 180.0;
    const cv::Point2d uDir(std::cos(a), std::sin(a));              // 扫描方向
    const cv::Point2d vDir(-std::sin(a), std::cos(a));             // 平行线方向

    ScanOptions opt;
    opt.polarity = propertyValue("edgePolarity").toInt();
    opt.gradThreshold = propertyValue("gradientThreshold").toDouble();
    opt.filterHalfWidth = propertyValue("filterHalfWidth").toInt();

    // 沿跨度方向每 1px 一条扫描线 (上限512条), 每条取起始边缘
    const int nLines = qBound(8, int(span), 512);
    std::vector<cv::Point2d> edgePts;
    edgePts.reserve(nLines);
    int foundCount = 0;
    for (int i = 0; i < nLines; ++i) {
        const double t = span * (i / double(nLines - 1) - 0.5);
        const cv::Point2d c = cv::Point2d(cx, cy) + t * vDir - (lineLen / 2) * uDir;
        SubpixEdgePoint ep;
        if (findEdgeSubpix(gray, c, c + lineLen * uDir, opt, ep)) {
            edgePts.push_back(ep.pos);
            ++foundCount;
        }
    }
    setResultData("scanLines", nLines);
    setResultData("edgesFound", foundCount);
    if (foundCount < 5) {
        setResultData("error", QStringLiteral("有效边缘点不足(%1/5) — 检查阈值/方向").arg(foundCount));
        setStatus(ToolStatus::NG);
        return false;
    }

    // 基线拟合 (鲁棒, 崩边段不会带偏基线)
    cv::Point2d dir, origin; double rms = 0;
    if (!fitLineRobust(edgePts, dir, origin, rms)) {
        setResultData("error", "基线拟合失败 (边缘退化)");
        setStatus(ToolStatus::NG);
        return false;
    }
    setResultData("baselineRms", rms);
    m_baseX1 = origin.x - dir.x * span; m_baseY1 = origin.y - dir.y * span;
    m_baseX2 = origin.x + dir.x * span; m_baseY2 = origin.y + dir.y * span;

    // 有符号偏离: 法线方向 (uDir 即基线法线 — 扫描方向垂直于边缘)
    // 偏离取"沿扫描方向离开基线"为正
    auto signedOffset = [&](const cv::Point2d& p) {
        return (p - origin).dot(uDir);
    };

    // 按扫描序号排序后找连续异常段
    const double depTh = propertyValue("depressionThreshold").toDouble();
    const int dirMode = propertyValue("depressionDir").toInt(); // 0=双向 1=仅凹(负) 2=仅凸(正)
    const double minW = propertyValue("minWidth").toDouble();
    // 线间距 (像素): 用于把"连续超阈值线数"换算为段宽
    const double step = span / double(nLines - 1);

    struct Run { int start = -1, end = -1; double peak = 0; };
    QVector<Run> runs;
    Run cur;
    for (int i = 0; i < int(edgePts.size()); ++i) {
        const double off = signedOffset(edgePts[i]);
        bool bad = std::abs(off) >= depTh;
        if (bad && dirMode == 1) bad = off < 0;
        if (bad && dirMode == 2) bad = off > 0;
        if (bad) {
            if (cur.start < 0) { cur = Run{ i, i, off }; }
            else { cur.end = i; cur.peak = std::abs(off) > std::abs(cur.peak) ? off : cur.peak; }
        } else if (cur.start >= 0) {
            runs.append(cur);
            cur = Run{};
        }
    }
    if (cur.start >= 0) runs.append(cur);

    // 段过滤: 最小宽度
    QVariantList segList;
    int segCount = 0;
    double maxDepth = 0;
    for (const Run& r : runs) {
        const double segWidth = (r.end - r.start + 1) * step;
        if (segWidth < minW) continue;
        ++segCount;
        maxDepth = std::max(maxDepth, std::abs(r.peak));
        // 段起止的图像坐标 (叠加层用)
        const cv::Point2d p1 = edgePts[qBound(0, r.start, int(edgePts.size()) - 1)];
        const cv::Point2d p2 = edgePts[qBound(0, r.end, int(edgePts.size()) - 1)];
        m_lastSegs.append({ p1.x, p1.y, p2.x, p2.y, r.peak });
        segList.append(QVariantList{ p1.x, p1.y, p2.x, p2.y, r.peak, segWidth });
    }

    setResultData("segmentCount", segCount);
    setResultData("maxDepth", maxDepth);
    setResultData("segments", segList);

    const int minCount = propertyValue("minCount").toInt();
    const int maxCount = propertyValue("maxCount").toInt();
    bool ok;
    if (minCount == 0 && maxCount == 0) ok = segCount == 0;      // 默认: 无异常段=OK
    else ok = segCount >= minCount && (maxCount == 0 || segCount <= maxCount);
    setResultData("ok", ok);
    m_lastOk = true;
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

std::vector<QVariant> EdgeDepression::overlays() const {
    std::vector<QVariant> out;
    if (!m_lastOk) return out;
    // 基线 (青色)
    QVariantMap base;
    base["type"] = "line";
    base["x1"] = m_baseX1; base["y1"] = m_baseY1;
    base["x2"] = m_baseX2; base["y2"] = m_baseY2;
    base["color"] = "#00c8ff";
    out.push_back(base);
    // 异常段 (红色, 深度>0 凸起橙色)
    for (const auto& s : m_lastSegs) {
        QVariantMap seg;
        seg["type"] = "line";
        seg["x1"] = s.x1; seg["y1"] = s.y1;
        seg["x2"] = s.x2; seg["y2"] = s.y2;
        seg["color"] = s.depth > 0 ? "#ff8800" : "#ff2200";
        out.push_back(seg);
    }
    return out;
}

VI_REGISTER_TOOL(EdgeDepression, "边缘凹陷", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector
