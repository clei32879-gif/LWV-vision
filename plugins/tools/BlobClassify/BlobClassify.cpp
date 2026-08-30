/**
 * @file BlobClassify.cpp
 * @brief 斑点分类工具 (对标 CKVision 斑点分类, P1-13 补齐)
 *
 * 对二值化连通域逐斑提取形状特征并做阈值判定:
 * - 面积 area / 周长 perimeter (轮廓弧长)
 * - 圆度 circularity = 4π·A / P²  (圆=1, 越扁越接近0)
 * - 长宽比 aspectRatio = 外接矩形宽/高 (>=1)
 * - 孔数 holeCount (内部闭合空洞数)
 *
 * 实现不依赖 findContours 的 RETR_CCOMP 层级 (OpenCV 5 该模式层级行为不可靠,
 * 各轮廓 parent 字段错乱): 用 RETR_EXTERNAL 取每个斑的外边界轮廓,
 * 孔数用"bbox 内反转图像 + 不与 bbox 边界接触的内含背景连通域"统计。
 *
 * 特征过滤 (面积/圆度/长宽比/孔数) 命中才算有效斑点;
 * 有效数在 [minCount, maxCount] 范围内 → OK。
 *
 * 输出: blobCount / totalArea / found / 主斑特征 / blobAreas 等
 */
#include "BlobClassify.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif
#include <map>
#include <algorithm>

namespace VisionInspector {

PropertyDefList BlobClassify::propertyDefs() const {
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
        // 特征过滤
        PropertyDef::boolProp("useAreaFilter", "启用面积过滤", false),
        PropertyDef::doubleProp("minArea", "面积下限", 10, 0, 1e9),
        PropertyDef::doubleProp("maxArea", "面积上限", 1e9, 0, 1e9),
        PropertyDef::boolProp("useCircularityFilter", "启用圆度过滤", false),
        PropertyDef::doubleProp("minCircularity", "圆度下限", 0.5, 0, 1),
        PropertyDef::boolProp("useAspectFilter", "启用长宽比过滤", false),
        PropertyDef::doubleProp("minAspectRatio", "长宽比下限", 1.0, 1, 100),
        PropertyDef::doubleProp("maxAspectRatio", "长宽比上限", 1.5, 1, 100),
        PropertyDef::boolProp("useHoleFilter", "启用孔数过滤", false),
        PropertyDef::intProp("minHoles", "孔数下限", 0, 0, 100),
        PropertyDef::intProp("maxHoles", "孔数上限", 100, 0, 100),
        // 判定
        PropertyDef::intProp("minCount", "有效斑点数下限", 1, 0, 100000),
        PropertyDef::intProp("maxCount", "有效斑点数上限", 100000, 0, 100000),
    };
}

namespace {
/** bbox 内反转后, 不与 bbox 边界接触的背景连通域个数 = 该斑的内部孔数 */
int countHoles(const cv::Mat& labels, int label, int left, int top, int w, int h) {
    if (w <= 0 || h <= 0) return 0;
    // 该斑掩码 (0/255)
    cv::Mat mask = (labels(cv::Rect(left, top, w, h)) == label);
    // 反转: 背景=255, 该斑=0
    cv::Mat back;
    cv::bitwise_not(mask, back);
    cv::Mat hlabels, hstats, hcent;
    const int n = cv::connectedComponentsWithStats(back, hlabels, hstats, hcent, 8, CV_32S);
    int holes = 0;
    for (int k = 1; k < n; ++k) {   // 0=bbox 外部背景
        const int hx = hstats.at<int>(k, cv::CC_STAT_LEFT);
        const int hy = hstats.at<int>(k, cv::CC_STAT_TOP);
        const int hw = hstats.at<int>(k, cv::CC_STAT_WIDTH);
        const int hh = hstats.at<int>(k, cv::CC_STAT_HEIGHT);
        // 与 bbox 边界接触 → 外部背景连通域 (bbox 是紧贴该斑的, 四周必有背景)
        const bool touchesBorder = (hx <= 0 || hy <= 0 || hx + hw >= w || hy + hh >= h);
        if (!touchesBorder) ++holes;
    }
    return holes;
}
} // namespace

bool BlobClassify::execute(ToolContext& context) {
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
        double ang = 0;
        applyCorrection(context, m_roi.centerX, m_roi.centerY, ang);
    }
    const int thresh = propertyValue("threshold").toInt();
    const bool autoThresh = propertyValue("autoThreshold").toBool();
    const int detType = propertyValue("detectionType").toInt();
    const int conn = propertyValue("connectivity").toInt() == 0 ? 4 : 8;

    const QRectF roiRect = m_roi.boundingRect();
    const int rx = std::max(0, (int)roiRect.x());
    const int ry = std::max(0, (int)roiRect.y());
    const int rw = std::min((int)roiRect.width(), src.cols - rx);
    const int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); setResultData("error", "ROI无效"); return false; }

    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    cv::Mat binary;
    if (autoThresh) cv::threshold(roiImg, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    else           cv::threshold(roiImg, binary, thresh, 255, cv::THRESH_BINARY);
    if (detType == 0) cv::bitwise_not(binary, binary);   // 黑色目标

    // 连通域 (4/8连通)
    cv::Mat labels, stats, centroids;
    const int ncomp = cv::connectedComponentsWithStats(binary, labels, stats, centroids,
                                                       (conn == 4) ? 4 : 8, CV_32S);

    // RETR_EXTERNAL: 每个斑一个外边界轮廓 (无层级嵌套)
    std::vector<std::vector<cv::Point>> outerContours;
    cv::findContours(binary, outerContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 特征过滤
    const bool useArea  = propertyValue("useAreaFilter").toBool();
    const double minArea = propertyValue("minArea").toDouble();
    const double maxArea = propertyValue("maxArea").toDouble();
    const bool useCirc  = propertyValue("useCircularityFilter").toBool();
    const double minCirc = propertyValue("minCircularity").toDouble();
    const bool useAsp   = propertyValue("useAspectFilter").toBool();
    const double minAsp = propertyValue("minAspectRatio").toDouble();
    const double maxAsp = propertyValue("maxAspectRatio").toDouble();
    const bool useHole  = propertyValue("useHoleFilter").toBool();
    const int minHoles = propertyValue("minHoles").toInt();
    const int maxHoles = propertyValue("maxHoles").toInt();
    const int minCount = propertyValue("minCount").toInt();
    const int maxCount = propertyValue("maxCount").toInt();

    // 外轮廓 → label 映射: 轮廓点落在哪个连通域上
    std::map<int, std::vector<cv::Point>> labelContour;
    for (const auto& c : outerContours) {
        if (c.empty() || cv::contourArea(c) <= 0) continue;
        int lbl = 0;
        for (const cv::Point& pt : c) {
            if (pt.y >= 0 && pt.y < labels.rows && pt.x >= 0 && pt.x < labels.cols) {
                const int v = labels.at<int>(pt.y, pt.x);
                if (v > 0) { lbl = v; break; }
            }
        }
        if (lbl > 0) labelContour[lbl] = c;
    }

    QVariantList blobAreas, blobPerimeters, blobCircularities, blobAspects, blobHoles;
    QVariantList blobCentersX, blobCentersY;
    double totalArea = 0;
    double largestArea = 0;
    int largestIdx = -1;

    for (int i = 1; i < ncomp; ++i) {   // 0=背景
        const int areaPix = stats.at<int>(i, cv::CC_STAT_AREA);
        if (areaPix < 1) continue;

        // bbox (孔数统计用)
        const int left = stats.at<int>(i, cv::CC_STAT_LEFT);
        const int top  = stats.at<int>(i, cv::CC_STAT_TOP);
        const int w    = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int h    = stats.at<int>(i, cv::CC_STAT_HEIGHT);

        double area = areaPix, perimeter = 0, circ = 0, aspect = 1.0;
        const auto it = labelContour.find(i);
        if (it != labelContour.end()) {
            const std::vector<cv::Point>& c = it->second;
            area = cv::contourArea(c);
            perimeter = cv::arcLength(c, true);
            // 圆度: 拟合椭圆长短轴比 (短轴/长轴)。对像素栅格化稳健:
            // 圆≈1, 椭圆 0~1, 矩形 0.3 左右 (理论 4πA/P² 受周长栅格化高估, 圆只能到 0.88)。
            if (c.size() >= 5) {
                const cv::RotatedRect ee = cv::fitEllipse(c);
                const double emaj = std::max(ee.size.width, ee.size.height);
                const double emin = std::min(ee.size.width, ee.size.height);
                circ = (emaj > 1e-9) ? (emin / emaj) : 0.0;
                circ = std::max(0.0, std::min(1.0, circ));
            }
            const cv::RotatedRect rr = cv::minAreaRect(c);
            const double cw = std::max(rr.size.width, rr.size.height);
            const double ch = std::min(rr.size.width, rr.size.height);
            aspect = (ch > 1e-9) ? (cw / ch) : 1.0;
        }
        const int holes = countHoles(labels, i, left, top, w, h);

        // 特征过滤
        if (useArea && (area < minArea || area > maxArea)) continue;
        if (useCirc && circ < minCirc) continue;
        if (useAsp && (aspect < minAsp || aspect > maxAsp)) continue;
        if (useHole && (holes < minHoles || holes > maxHoles)) continue;

        blobAreas.append(area);
        blobPerimeters.append(perimeter);
        blobCircularities.append(circ);
        blobAspects.append(aspect);
        blobHoles.append(holes);
        const double cx = centroids.at<double>(i, 0) + rx;
        const double cy = centroids.at<double>(i, 1) + ry;
        blobCentersX.append(cx);
        blobCentersY.append(cy);
        totalArea += area;
        if (area > largestArea) { largestArea = area; largestIdx = (int)blobAreas.size() - 1; }
    }

    const int blobCount = (int)blobAreas.size();
    setResultData("blobCount", blobCount);
    setResultData("totalArea", totalArea);
    setResultData("avgArea", blobCount > 0 ? totalArea / blobCount : 0);
    setResultData("blobAreas", blobAreas);
    setResultData("blobPerimeters", blobPerimeters);
    setResultData("blobCircularities", blobCircularities);
    setResultData("blobAspects", blobAspects);
    setResultData("blobHoles", blobHoles);
    setResultData("blobCentersX", blobCentersX);
    setResultData("blobCentersY", blobCentersY);
    if (largestIdx >= 0) {
        setResultData("mainArea", blobAreas[largestIdx].toDouble());
        setResultData("mainPerimeter", blobPerimeters[largestIdx].toDouble());
        setResultData("mainCircularity", blobCircularities[largestIdx].toDouble());
        setResultData("mainAspectRatio", blobAspects[largestIdx].toDouble());
        setResultData("mainHoles", blobHoles[largestIdx].toInt());
        setResultData("mainCenterX", blobCentersX[largestIdx].toDouble());
        setResultData("mainCenterY", blobCentersY[largestIdx].toDouble());
    }

    const bool ok = (blobCount >= minCount && blobCount <= maxCount);
    setResultData("found", ok);
    setResultData("judgment", ok ? "OK" : "NG");
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(BlobClassify, "斑点分类", VisionInspector::ToolCategory::Detection)
