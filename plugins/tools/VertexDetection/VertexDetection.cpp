#include "VertexDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif
#include <cmath>

namespace VisionInspector {

PropertyDefList VertexDetection::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0),
        PropertyDef::enumProp("detectPosition", "检测位置", {"起始", "最近", "最远", "最强"}, 0),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 40, 1, 255),
        PropertyDef::intProp("filterHalfWidth", "滤波半宽", 1, 1, 20),
        PropertyDef::intProp("scanWidth", "扫描宽度", 1, 1, 100),
    };
}

bool VertexDetection::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    QMap<QString, QVariant> roiProps;
    roiProps["roiType"] = propertyValue("roiType").toInt();
    m_roi = ROIRegion::fromProperties(roiProps);
    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.width = propertyValue("roiWidth").toDouble();
    m_roi.height = propertyValue("roiHeight").toDouble();
    {
        double ang = 0;   // VertexDetection 无角度
        applyCorrection(context, m_roi.centerX, m_roi.centerY, ang);   // 位置补正跟随
    }
    int polarity = propertyValue("edgePolarity").toInt();            // 0任意 1亮到暗 2暗到亮
    int threshold = propertyValue("gradientThreshold").toInt();
    int scanWidth = std::max(1, propertyValue("scanWidth").toInt()); // 扫描行步长
    const int detectPos = propertyValue("detectPosition").toInt();     // 0起始 1最近 2最远 3最强
    const int filterHalf = std::max(1, propertyValue("filterHalfWidth").toInt());
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); return false; }
    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    // #2 形状ROI: 角点只在菱形/圆形内部找
    const cv::Mat shapeMask = makeRoiShapeMask(m_roi, roiRect);
    // 用 filterHalfWidth 平滑后再求梯度 (抑制噪点)
    cv::Mat smoothed;
    cv::boxFilter(roiImg, smoothed, -1, cv::Size(filterHalf * 2 + 1, filterHalf * 2 + 1),
                  cv::Point(-1, -1), true, cv::BORDER_REPLICATE);
    // 扫描 ROI 找角点候选 (角点响应 = |dx*dy|: 角点处水平+垂直梯度同时显著, 边缘处趋于0)
    struct Cand { double x; double y; double score; };
    std::vector<Cand> cands;
    const int cxs = rw / 4, cxe = rw * 3 / 4, cys = rh / 4, cye = rh * 3 / 4;
    for (int x = cxs; x < cxe; ++x) {
        for (int y = cys; y < cye; y += scanWidth) {
            if (!shapeMask.empty() && shapeMask.at<uchar>(y, x) == 0) continue;
            const double l = (x > 0) ? smoothed.at<uchar>(y, x - 1) : smoothed.at<uchar>(y, x);
            const double r = (x < rw - 1) ? smoothed.at<uchar>(y, x + 1) : smoothed.at<uchar>(y, x);
            const double u = (y > 0) ? smoothed.at<uchar>(y - 1, x) : smoothed.at<uchar>(y, x);
            const double dn = (y < rh - 1) ? smoothed.at<uchar>(y + 1, x) : smoothed.at<uchar>(y, x);
            const double dx = r - l, dy = dn - u;
            // 极性过滤: 亮到暗(左->右梯度为负, 取-r+l) / 暗到亮 / 任意
            if (polarity == 1 && dx >= 0) continue;
            if (polarity == 2 && dx <= 0) continue;
            const double score = std::fabs(dx * dy);
            if (score > threshold)
                cands.push_back({(double)x, (double)y, score});
        }
    }
    if (cands.empty()) { setResultData("found", false); setStatus(ToolStatus::NG); return false; }

    // 按检测位置策略选点: 起始(扫描序首个) / 最近(离ROI中心) / 最远 / 最强(默认)
    const double cxc = (cxs + cxe) / 2.0, cyc = (cys + cye) / 2.0;
    const Cand* pick = nullptr;
    for (const auto& c : cands) {
        if (detectPos == 0) { pick = &c; break; }              // 起始
        if (detectPos == 1 && (pick == nullptr ||
            std::hypot(c.x - cxc, c.y - cyc) < std::hypot(pick->x - cxc, pick->y - cyc)))
            pick = &c;                                          // 最近
        if (detectPos == 2 && (pick == nullptr ||
            std::hypot(c.x - cxc, c.y - cyc) > std::hypot(pick->x - cxc, pick->y - cyc)))
            pick = &c;                                          // 最远
        if (detectPos == 3 && (pick == nullptr || c.score > pick->score))
            pick = &c;                                          // 最强
    }
    if (!pick) pick = &cands.front();
    setResultData("positionX", rx + pick->x);
    setResultData("positionY", ry + pick->y);
    setResultData("distance", std::sqrt(pick->score));
    setResultData("candidates", (int)cands.size());
    setResultData("found", true);
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(VertexDetection, "检测顶点", VisionInspector::ToolCategory::Detection)
