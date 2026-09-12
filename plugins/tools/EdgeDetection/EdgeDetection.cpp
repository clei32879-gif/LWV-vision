#include "EdgeDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif

namespace VisionInspector {

PropertyDefList EdgeDetection::propertyDefs() const {
    return {
        // ROI参数
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形", "菱形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::doubleProp("roiAngle", "ROI角度", 0, -180, 180),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "定位"),
        // 检测参数
        PropertyDef::enumProp("edgePolarity", "边缘极性", {"任意", "亮到暗", "暗到亮"}, 0),
        PropertyDef::enumProp("edgePosition", "边缘位置", {"起始位置", "最近", "最远", "最强", "全部"}, 0),
        PropertyDef::intProp("gradientThreshold", "梯度阈值", 40, 1, 255),
        PropertyDef::intProp("filterHalfWidth", "滤波半宽", 1, 1, 20),
        PropertyDef::intProp("scanWidth", "扫描宽度", 1, 1, 100),
    };
}

bool EdgeDetection::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    // 创建ROI
    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.width = propertyValue("roiWidth").toDouble();
    m_roi.height = propertyValue("roiHeight").toDouble();
    m_roi.angle = propertyValue("roiAngle").toDouble();
    m_roi.type = (ROIType)propertyValue("roiType").toInt();   // 形状ROI激活(菱形/圆形/环形)
    applyCorrection(context, m_roi.centerX, m_roi.centerY, m_roi.angle);   // 位置补正跟随

    int polarity = propertyValue("edgePolarity").toInt();
    int position = propertyValue("edgePosition").toInt();
    int threshold = propertyValue("gradientThreshold").toInt();
    int filterWidth = propertyValue("filterHalfWidth").toInt();
    int scanWidth = propertyValue("scanWidth").toInt();

    // 提取ROI区域图像
    QRectF roiRect = m_roi.boundingRect();
    int rx = std::max(0, (int)roiRect.x());
    int ry = std::max(0, (int)roiRect.y());
    int rw = std::min((int)roiRect.width(), src.cols - rx);
    int rh = std::min((int)roiRect.height(), src.rows - ry);
    if (rw <= 0 || rh <= 0) { setStatus(ToolStatus::NG); setResultData("error", "ROI区域无效"); return false; }

    cv::Mat roiImg = src(cv::Rect(rx, ry, rw, rh)).clone();
    // #2 形状ROI: 扫描带只统计形状内部像素 (菱形/圆形/环形)
    const cv::Mat bandMask = makeRoiShapeMask(m_roi, roiRect);

    // 沿扫描方向采样
    std::vector<double> profile;
    for (int x = 0; x < rw; ++x) {
        double sum = 0;
        int count = 0;
        for (int dy = -scanWidth/2; dy <= scanWidth/2; ++dy) {
            int y = rh/2 + dy;
            if (y >= 0 && y < rh) {
                if (!bandMask.empty() && bandMask.at<uchar>(y, x) == 0) continue;
                sum += roiImg.at<uchar>(y, x);
                count++;
            }
        }
        profile.push_back(count > 0 ? sum / count : 0);
    }

    // M-42: filterHalfWidth 参与平滑(移动平均), 此前读取后未使用
    if (filterWidth > 1 && (int)profile.size() > filterWidth * 2) {
        std::vector<double> smoothed(profile.size());
        for (size_t i = 0; i < profile.size(); ++i) {
            double s = 0; int n = 0;
            int lo = (int)i - filterWidth, hi = (int)i + filterWidth;
            if (lo < 0) lo = 0;
            if (hi >= (int)profile.size()) hi = (int)profile.size() - 1;
            for (int k = lo; k <= hi; ++k) { s += profile[k]; ++n; }
            smoothed[i] = n > 0 ? s / n : profile[i];
        }
        profile = std::move(smoothed);
    }

    // 计算梯度
    std::vector<double> gradient(profile.size());
    for (size_t i = 1; i < profile.size() - 1; ++i) {
        gradient[i] = (profile[i+1] - profile[i-1]) / 2.0;
    }

    // 查找边缘
    struct EdgePoint { double x; double y; double grad; };
    std::vector<EdgePoint> edges;
    for (size_t i = 1; i < gradient.size() - 1; ++i) {
        bool match = false;
        if (polarity == 0) match = std::abs(gradient[i]) >= threshold;
        else if (polarity == 1) match = gradient[i] <= -threshold;
        else match = gradient[i] >= threshold;
        if (match) {
            edges.push_back({rx + (double)i, ry + (double)rh/2, gradient[i]});
        }
    }

    // 根据位置选择边缘
    EdgePoint selectedEdge = {0, 0, 0};
    bool found = false;
    if (!edges.empty()) {
        if (position == 4) {   // 全部: 输出全部边缘点列表 (M-42)
            QVariantList all;
            for (const auto& e : edges) {
                // 显式包QVariant强制嵌套: 直接append(QVariantList)会命中QList::append(QList<T>)重载拍平
                all.append(QVariant(QVariantList{e.x, e.y, e.grad}));
            }
            setResultData("edgePoints", all);
            selectedEdge = edges.front();
            found = true;
        }
        else if (position == 0 || position == 1) { selectedEdge = edges.front(); found = true; }
        else if (position == 2) { selectedEdge = edges.back(); found = true; }
        else if (position == 3) {
            double maxGrad = 0;
            for (const auto& e : edges) {
                if (std::abs(e.grad) > maxGrad) { maxGrad = std::abs(e.grad); selectedEdge = e; }
            }
            found = true;
        }
        else { selectedEdge = edges.front(); found = true; }
    }

    setResultData("edgeCount", (int)edges.size());
    setResultData("positionX", selectedEdge.x);
    setResultData("positionY", selectedEdge.y);
    setResultData("distance", selectedEdge.x - rx);
    setResultData("found", found);

    setStatus(found ? ToolStatus::OK : ToolStatus::NG);
    return found;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(EdgeDetection, "边缘检测", VisionInspector::ToolCategory::Detection)
