/**
 * @file EdgeSpacing.cpp
 * @brief 检测间距工具 (对标 CKVision 检测间距, P1-12 补齐)
 *
 * 沿卡尺扫描线检测全部边缘点, 计算相邻边缘间距 (gap):
 * - 边缘位置模式: 起始(第一个间距)/最后(最后一个)/最宽(距离最大)/全部
 * - 间距宽度 = 两相邻边缘沿扫描方向的距离; 中心位置 = 中点
 * 判定: 报告间距宽度在 [minWidth, maxWidth] → OK
 *
 * 输出: gapCount/gapWidths/gapCentersX/gapCentersY + 主间距 mainWidth/mainCenter
 */
#include "EdgeSpacing.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <cmath>
#include <vector>

namespace VisionInspector {

PropertyDefList EdgeSpacing::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 1),
        PropertyDef::doubleProp("roiCenterX", "扫描中心X", 320, 0, 10000, "卡尺"),
        PropertyDef::doubleProp("roiCenterY", "扫描中心Y", 240, 0, 10000, "卡尺"),
        PropertyDef::doubleProp("roiWidth", "扫描长度", 120, 1, 10000, "卡尺"),
        PropertyDef::doubleProp("roiAngle", "扫描角度", 0, -180, 180, "卡尺"),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "卡尺"),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0, "检测"),
        PropertyDef::enumProp("edgePosition", "边缘位置", {"起始", "最后", "最宽", "全部"}, 0, "检测"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 30, 1, 255, "检测"),
        PropertyDef::intProp("filterHalfWidth", "梯度平滑半宽", 2, 1, 20, "检测"),
        PropertyDef::doubleProp("minWidth", "间距宽度下限", 0, 0, 10000),
        PropertyDef::doubleProp("maxWidth", "间距宽度上限", 10000, 0, 10000),
    };
}

bool EdgeSpacing::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = input->clone();

    double cx = propertyValue("roiCenterX").toDouble();
    double cy = propertyValue("roiCenterY").toDouble();
    const double roiW = propertyValue("roiWidth").toDouble();
    double roiAng = propertyValue("roiAngle").toDouble();
    applyCorrection(context, cx, cy, roiAng);

    ScanOptions opt;
    opt.polarity = propertyValue("edgePolarity").toInt();
    opt.gradThreshold = propertyValue("gradientThreshold").toInt();
    opt.filterHalfWidth = propertyValue("filterHalfWidth").toInt();

    const double rad = roiAng * CV_PI / 180.0;
    const cv::Point2d u(std::cos(rad), std::sin(rad));
    const cv::Point2d p0(cx - u.x * roiW / 2, cy - u.y * roiW / 2);
    const cv::Point2d p1(cx + u.x * roiW / 2, cy + u.y * roiW / 2);

    std::vector<SubpixEdgePoint> edges = findEdgesSubpix(src, p0, p1, opt, 0);
    // 去重: 移除相邻距离 < 1px 的重复边缘 (正负梯度在同位置产生两个峰)
    {
        std::vector<SubpixEdgePoint> unique;
        for (const auto& e : edges) {
            if (unique.empty() || std::hypot(e.pos.x - unique.back().pos.x,
                                              e.pos.y - unique.back().pos.y) >= 1.0)
                unique.push_back(e);
        }
        edges.swap(unique);
    }

    // 相邻边缘间距
    QVariantList gapWidths, gapCentersX, gapCentersY;
    for (size_t i = 1; i < edges.size(); ++i) {
        const double w = std::hypot(edges[i].pos.x - edges[i - 1].pos.x,
                                    edges[i].pos.y - edges[i - 1].pos.y);
        const double mx = (edges[i].pos.x + edges[i - 1].pos.x) / 2;
        const double my = (edges[i].pos.y + edges[i - 1].pos.y) / 2;
        gapWidths.append(w);
        gapCentersX.append(mx);
        gapCentersY.append(my);
    }

    const int mode = propertyValue("edgePosition").toInt();
    int idx = -1;
    if (gapWidths.isEmpty()) {
        setResultData("gapCount", 0);
        setResultData("found", false);
        setResultData("judgment", "NG");
        setStatus(ToolStatus::NG);
        return false;
    }
    if (mode == 0) idx = 0;
    else if (mode == 1) idx = (int)gapWidths.size() - 1;
    else {
        double maxW = -1;
        for (int i = 0; i < (int)gapWidths.size(); ++i) {
            if (gapWidths[i].toDouble() > maxW) { maxW = gapWidths[i].toDouble(); idx = i; }
        }
    }

    const double mainWidth = gapWidths[idx].toDouble();
    const double minW = propertyValue("minWidth").toDouble();
    const double maxW2 = propertyValue("maxWidth").toDouble();
    const bool ok = (mainWidth >= minW && mainWidth <= maxW2);

    setResultData("gapCount", (int)gapWidths.size());
    setResultData("gapWidths", gapWidths);
    setResultData("gapCentersX", gapCentersX);
    setResultData("gapCentersY", gapCentersY);
    setResultData("mainWidth", mainWidth);
    setResultData("mainCenterX", gapCentersX[idx].toDouble());
    setResultData("mainCenterY", gapCentersY[idx].toDouble());
    setResultData("found", true);
    setResultData("judgment", ok ? "OK" : "NG");
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(EdgeSpacing, "检测间距", VisionInspector::ToolCategory::Geometry)
