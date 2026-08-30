/**
 * @file ScanEdge.cpp
 * @brief 扫描边缘工具 (对标 CKVision 扫描边缘, P1-12 补齐)
 *
 * 沿卡尺扫描线 (或扫描数量条平行线) 检测全部边缘点:
 * - 边缘位置模式: 起始(最靠扫描起点)/最后(最远)/最强(梯度最大)
 * - 扫描数量: 并行扫描线数, 扫描宽度: 单条线平均高
 * 输出: edgeCount/edgeX/edgeY 列表 + 主边缘 mainX/mainY/mainStrength
 */
#include "ScanEdge.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/SubpixEdge.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <cmath>
#include <algorithm>
#include <vector>

namespace VisionInspector {

PropertyDefList ScanEdge::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 1),
        PropertyDef::doubleProp("roiCenterX", "扫描中心X", 320, 0, 10000, "卡尺"),
        PropertyDef::doubleProp("roiCenterY", "扫描中心Y", 240, 0, 10000, "卡尺"),
        PropertyDef::doubleProp("roiWidth", "扫描长度", 120, 1, 10000, "卡尺"),
        PropertyDef::doubleProp("roiHeight", "扫描宽度(平均高)", 10, 1, 1000, "卡尺"),
        PropertyDef::doubleProp("roiAngle", "扫描角度", 0, -180, 180, "卡尺"),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "卡尺"),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0, "检测"),
        PropertyDef::enumProp("edgePosition", "边缘位置", {"起始", "最后", "最强"}, 0, "检测"),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 30, 1, 255, "检测"),
        PropertyDef::intProp("filterHalfWidth", "梯度平滑半宽", 2, 1, 20, "检测"),
        PropertyDef::intProp("scanCount", "扫描数量", 1, 1, 50, "检测"),
    };
}

bool ScanEdge::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = input->clone();

    double cx = propertyValue("roiCenterX").toDouble();
    double cy = propertyValue("roiCenterY").toDouble();
    const double roiW = propertyValue("roiWidth").toDouble();
    const double roiH = propertyValue("roiHeight").toDouble();
    double roiAng = propertyValue("roiAngle").toDouble();
    applyCorrection(context, cx, cy, roiAng);

    ScanOptions opt;
    opt.polarity = propertyValue("edgePolarity").toInt();
    opt.gradThreshold = propertyValue("gradientThreshold").toInt();
    opt.filterHalfWidth = propertyValue("filterHalfWidth").toInt();
    const int scanCount = std::max(1, propertyValue("scanCount").toInt());

    const double rad = roiAng * CV_PI / 180.0;
    const cv::Point2d u(std::cos(rad), std::sin(rad));
    const cv::Point2d n(-std::sin(rad), std::cos(rad));

    // 并行扫描线: 在垂直方向等距偏移
    std::vector<SubpixEdgePoint> allEdges;
    const double totalH = std::max(roiH, (double)scanCount);
    for (int s = 0; s < scanCount; ++s) {
        double off = 0;
        if (scanCount > 1) off = (s - (scanCount - 1) / 2.0) * (totalH / scanCount);
        const cv::Point2d center = cv::Point2d(cx, cy) + n * off;
        const cv::Point2d p0 = center - u * (roiW / 2);
        const cv::Point2d p1 = center + u * (roiW / 2);
        std::vector<SubpixEdgePoint> es = findEdgesSubpix(src, p0, p1, opt, 0);
        // 去重: 移除相邻距离 < 1px 的重复边缘
        for (size_t i = 1; i < es.size(); ) {
            if (std::hypot(es[i].pos.x - es[i-1].pos.x, es[i].pos.y - es[i-1].pos.y) < 1.0)
                es.erase(es.begin() + i);
            else ++i;
        }
        allEdges.insert(allEdges.end(), es.begin(), es.end());
    }

    // 按扫描方向排序 (投影到扫描轴)
    const auto proj = [&](const SubpixEdgePoint& e) {
        return (e.pos.x - cx) * u.x + (e.pos.y - cy) * u.y;
    };
    std::sort(allEdges.begin(), allEdges.end(),
              [&](const SubpixEdgePoint& a, const SubpixEdgePoint& b) { return proj(a) < proj(b); });

    QVariantList edgeX, edgeY;
    for (const auto& e : allEdges) {
        edgeX.append(e.pos.x);
        edgeY.append(e.pos.y);
    }

    if (allEdges.empty()) {
        setResultData("edgeCount", 0);
        setResultData("found", false);
        setResultData("judgment", "NG");
        setStatus(ToolStatus::NG);
        return false;
    }

    const int mode = propertyValue("edgePosition").toInt();
    int idx = 0;
    if (mode == 0) idx = 0;
    else if (mode == 1) idx = (int)allEdges.size() - 1;
    else {
        double maxS = -1;
        for (size_t i = 0; i < allEdges.size(); ++i) {
            if (allEdges[i].strength > maxS) { maxS = allEdges[i].strength; idx = (int)i; }
        }
    }

    setResultData("edgeCount", (int)allEdges.size());
    setResultData("edgeX", edgeX);
    setResultData("edgeY", edgeY);
    setResultData("mainX", allEdges[idx].pos.x);
    setResultData("mainY", allEdges[idx].pos.y);
    setResultData("mainStrength", allEdges[idx].strength);
    setResultData("found", true);
    setResultData("judgment", "OK");
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ScanEdge, "扫描边缘", VisionInspector::ToolCategory::Geometry)
