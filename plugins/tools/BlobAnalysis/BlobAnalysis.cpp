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
    // M-19修复: findContours两分支相同导致连通性失效;
    // 改用connectedComponentsWithStats原生支持4/8连通
    cv::Mat labels, stats, centroids;
    const int ncomp = cv::connectedComponentsWithStats(binary, labels, stats, centroids,
                                                       (conn == 4) ? 4 : 8, CV_32S);
    const bool wantEllipse = propertyValue("useEllipse").toBool();
    const bool wantBBox = propertyValue("useBBox").toBool();
    int blobCount = 0;
    double totalArea = 0;
    int largestIdx = -1;
    double largestArea = 0;
    for (int i = 1; i < ncomp; ++i) {   // 0=背景
        const double area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= minArea) {
            blobCount++; totalArea += area;
            if (area > largestArea) { largestArea = area; largestIdx = i; }
        }
    }
    setResultData("blobCount", blobCount);
    setResultData("totalArea", totalArea);
    setResultData("avgArea", blobCount > 0 ? totalArea / blobCount : 0);
    setResultData("found", blobCount > 0);

    if (largestIdx > 0) {
        // 主斑面积/质心 (已含ROI偏移)
        const double cx = centroids.at<double>(largestIdx, 0) + rx;
        const double cy = centroids.at<double>(largestIdx, 1) + ry;
        setResultData("mainArea", largestArea);
        setResultData("mainCenterX", cx);
        setResultData("mainCenterY", cy);
        if (wantBBox) {
            // 最小外接矩形 (含角度, 规范化为 0-180 主轴方向)
            cv::Mat mask = (labels == largestIdx);
            std::vector<std::vector<cv::Point>> cs;
            cv::findContours(mask, cs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            if (!cs.empty()) {
                const cv::RotatedRect rr = cv::minAreaRect(cs[0]);
                // OpenCV: width>=height, angle∈[-90,0] → 主轴方向角 = angle<0 ? angle+90 : angle
                double normAngle = rr.angle;
                if (normAngle < 0) normAngle += 90.0;
                setResultData("bboxCenterX", rr.center.x + rx);
                setResultData("bboxCenterY", rr.center.y + ry);
                setResultData("bboxWidth", std::max(rr.size.width, rr.size.height));
                setResultData("bboxHeight", std::min(rr.size.width, rr.size.height));
                setResultData("bboxAngle", normAngle);
            }
        }
        if (wantEllipse) {
            // 主轴椭圆: 用最小外接矩形尺寸近似长短轴
            cv::Mat mask = (labels == largestIdx);
            std::vector<std::vector<cv::Point>> cs;
            cv::findContours(mask, cs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            if (!cs.empty() && cs[0].size() >= 5) {
                const cv::RotatedRect rr = cv::fitEllipse(cs[0]);
                setResultData("ellipseCenterX", rr.center.x + rx);
                setResultData("ellipseCenterY", rr.center.y + ry);
                setResultData("ellipseMajor", std::max(rr.size.width, rr.size.height));
                setResultData("ellipseMinor", std::min(rr.size.width, rr.size.height));
                setResultData("ellipseAngle", rr.angle);
            }
        }
    }
    setStatus(blobCount > 0 ? ToolStatus::OK : ToolStatus::NG);
    return blobCount > 0;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(BlobAnalysis, "斑点分析", VisionInspector::ToolCategory::Detection)
