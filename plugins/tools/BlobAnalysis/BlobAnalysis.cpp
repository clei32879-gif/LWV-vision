#include "BlobAnalysis.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif

namespace VisionInspector {

PropertyDefList BlobAnalysis::propertyDefs() const {
    return {
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形", "圆形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        PropertyDef::intProp("threshold", "阈值", 127, 0, 255),
        PropertyDef::boolProp("autoThreshold", "自动计算阈值", true),
        PropertyDef::enumProp("detectionType", "检测类型", {"黑色", "白色"}, 0),
        PropertyDef::enumProp("connectivity", "连通性", {"四连通", "八连通"}, 0),
        PropertyDef::intProp("minArea", "限定面积", 5, 0, 1000000),
        PropertyDef::boolProp("useEllipse", "主轴椭圆特征", false),
        PropertyDef::boolProp("useBBox", "最小外接矩形特征", false),
    };
}

bool BlobAnalysis::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;
    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.width = propertyValue("roiWidth").toDouble();
    m_roi.height = propertyValue("roiHeight").toDouble();
    {
        double ang = 0;   // Blob 无角度
        applyCorrection(context, m_roi.centerX, m_roi.centerY, ang);   // 位置补正跟随
    }
    int thresh = propertyValue("threshold").toInt();
    bool autoThresh = propertyValue("autoThreshold").toBool();
    int detType = propertyValue("detectionType").toInt();
    int conn = propertyValue("connectivity").toInt() == 0 ? 4 : 8;
    int minArea = propertyValue("minArea").toInt();
    // 提取ROI
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); return false; }
    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    cv::Mat binary;
    if (autoThresh) {
        cv::threshold(roiImg, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    } else {
        cv::threshold(roiImg, binary, thresh, 255, cv::THRESH_BINARY);
    }
    if (detType == 0) cv::bitwise_not(binary, binary); // 黑色目标
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, (conn == 4) ? cv::CHAIN_APPROX_SIMPLE : cv::CHAIN_APPROX_SIMPLE);
    int blobCount = 0;
    double totalArea = 0;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area >= minArea) { blobCount++; totalArea += area; }
    }
    setResultData("blobCount", blobCount);
    setResultData("totalArea", totalArea);
    setResultData("avgArea", blobCount > 0 ? totalArea / blobCount : 0);
    setResultData("found", blobCount > 0);
    setStatus(blobCount > 0 ? ToolStatus::OK : ToolStatus::NG);
    return blobCount > 0;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(BlobAnalysis, "斑点分析", VisionInspector::ToolCategory::Detection)
