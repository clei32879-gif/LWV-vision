/**
 * @file PixelStatistics.cpp
 * @brief 像素统计工具 (对标 CKVision 像素统计, P1-12 补齐)
 *
 * 对 ROI 内图像做两类统计:
 * 1. 灰度基础统计: 均值/标准差/最小/最大 (对应 CKVision 检测亮度)
 * 2. 阈值像素统计: [低阈值, 高阈值] 区间内像素数 + 比率 (对应 CKVision 像素统计)
 * 判定: 像素数/比率均在 [min, max] → OK
 *
 * 输出: mean/stddev/min/max/whiteCount/whiteRatio/roiArea + judgment
 */
#include "PixelStatistics.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList PixelStatistics::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        // 阈值统计
        PropertyDef::intProp("lowThreshold", "低阈值", 100, 0, 255),
        PropertyDef::intProp("highThreshold", "高阈值", 255, 0, 255),
        // 判定
        PropertyDef::doubleProp("minCount", "像素数下限", 0, 0, 1e9),
        PropertyDef::doubleProp("maxCount", "像素数上限", 1e9, 0, 1e9),
        PropertyDef::doubleProp("minRatio", "比率下限", 0.0, 0, 1),
        PropertyDef::doubleProp("maxRatio", "比率上限", 1.0, 0, 1),
    };
}

bool PixelStatistics::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = input->clone();   // 克隆, 后续只读不污染

    // ROI 裁剪
    if (propertyValue("roiType").toInt() > 0) {
        m_roi.type = ROIType::Rectangle;
        m_roi.centerX = propertyValue("roiCenterX").toDouble();
        m_roi.centerY = propertyValue("roiCenterY").toDouble();
        m_roi.width = propertyValue("roiWidth").toDouble();
        m_roi.height = propertyValue("roiHeight").toDouble();
        double ang = 0;
        applyCorrection(context, m_roi.centerX, m_roi.centerY, ang);
        const QRectF rr = m_roi.boundingRect();
        const int x = std::max(0, (int)rr.x());
        const int y = std::max(0, (int)rr.y());
        const int w = std::min((int)rr.width(), src.cols - x);
        const int h = std::min((int)rr.height(), src.rows - y);
        if (w > 0 && h > 0) src = src(cv::Rect(x, y, w, h)).clone();
    }
    if (src.empty()) { setStatus(ToolStatus::NG); setResultData("error", "ROI为空"); return false; }

    // 1. 基础统计
    cv::Scalar mean, stddev;
    cv::meanStdDev(src, mean, stddev);
    double mn = 0, mx = 0;
    cv::minMaxLoc(src, &mn, &mx);
    const double avg = mean[0];
    const double sd = stddev[0];

    // 2. 阈值区间像素
    const int low = propertyValue("lowThreshold").toInt();
    const int high = propertyValue("highThreshold").toInt();
    const int lo = std::min(low, high), hi = std::max(low, high);
    cv::Mat mask;
    cv::inRange(src, cv::Scalar(lo), cv::Scalar(hi), mask);
    const double whiteCount = (double)cv::countNonZero(mask);
    const double roiArea = (double)src.total();
    const double whiteRatio = roiArea > 0 ? whiteCount / roiArea : 0;

    const double minCount = propertyValue("minCount").toDouble();
    const double maxCount = propertyValue("maxCount").toDouble();
    const double minRatio = propertyValue("minRatio").toDouble();
    const double maxRatio = propertyValue("maxRatio").toDouble();
    const bool ok = (whiteCount >= minCount && whiteCount <= maxCount &&
                     whiteRatio >= minRatio && whiteRatio <= maxRatio);

    setResultData("mean", avg);
    setResultData("stddev", sd);
    setResultData("min", mn);
    setResultData("max", mx);
    setResultData("whiteCount", whiteCount);
    setResultData("whiteRatio", whiteRatio);
    setResultData("roiArea", roiArea);
    setResultData("judgment", ok ? "OK" : "NG");
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(PixelStatistics, "像素统计", VisionInspector::ToolCategory::Detection)