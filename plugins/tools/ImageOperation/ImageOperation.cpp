/**
 * @file ImageOperation.cpp
 * @brief 图像运算工具 (对标 CKVision 图像运算, P1-11 补齐)
 *
 * 对两张图像逐像素运算并输出一张新图像:
 * 加/减/差分(absdiff)/与/或/异或/最小/最大/平均。
 * - 输入图像1: getInputImage (inputImage 属性, 默认 Current)
 * - 输入图像2: inputImage2 属性 (引用工具实例名/命名图像槽)
 * - 尺寸不一致时以图像1为准裁剪/缩放到图像2对齐; 支持 ROI 裁剪后运算
 *
 * 输出: outputWidth/outputHeight + 写入 Current 供后续工具引用
 */
#include "ImageOperation.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList ImageOperation::propertyDefs() const {
    return {
        PropertyDef::stringProp("inputImage2", "输入图像2", ""),
        PropertyDef::enumProp("operation", "运算", {"加", "减", "差分", "与", "或", "异或", "最小值", "最大值", "平均"}, 0),
        // ROI (裁剪运算区域)
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 0),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
    };
}

bool ImageOperation::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input1 = getInputImage(context);
    if (!input1 || input1->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像1为空"); return false; }

    const QString src2Name = propertyValue("inputImage2").toString();
    CvImagePtr input2;
    if (!src2Name.isEmpty() && src2Name != "Current") {
        input2 = context.getImage(src2Name);
    }
    if (!input2) input2 = context.currentImage();
    if (!input2 || input2->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像2为空"); return false; }

    cv::Mat a, b;
    if (input1->channels() > 1) cv::cvtColor(*input1, a, cv::COLOR_BGR2GRAY);
    else a = *input1;
    if (input2->channels() > 1) cv::cvtColor(*input2, b, cv::COLOR_BGR2GRAY);
    else b = *input2;

    // ROI 裁剪 (对图像1; 图像2按其尺寸对齐)
    m_roi.centerX = propertyValue("roiCenterX").toDouble();
    m_roi.centerY = propertyValue("roiCenterY").toDouble();
    m_roi.width = propertyValue("roiWidth").toDouble();
    m_roi.height = propertyValue("roiHeight").toDouble();
    if (propertyValue("roiType").toInt() > 0) {
        double ang = 0;
        applyCorrection(context, m_roi.centerX, m_roi.centerY, ang);
        const QRectF rr = m_roi.boundingRect();
        const int x = std::max(0, (int)rr.x());
        const int y = std::max(0, (int)rr.y());
        const int w = std::min((int)rr.width(), a.cols - x);
        const int h = std::min((int)rr.height(), a.rows - y);
        if (w > 0 && h > 0) a = a(cv::Rect(x, y, w, h)).clone();
    }

    // 尺寸对齐: 以图像1为准
    if (a.size() != b.size() || a.type() != b.type()) {
        if (b.type() != a.type()) b.convertTo(b, a.type());
        if (b.size() != a.size()) cv::resize(b, b, a.size(), 0, 0, cv::INTER_LINEAR);
    }

    const int op = propertyValue("operation").toInt();
    cv::Mat result;
    switch (op) {
    case 0: cv::add(a, b, result); break;                       // 加 (饱和)
    case 1: cv::subtract(a, b, result); break;                  // 减 (饱和)
    case 2: cv::absdiff(a, b, result); break;                   // 差分
    case 3: cv::bitwise_and(a, b, result); break;
    case 4: cv::bitwise_or(a, b, result); break;
    case 5: cv::bitwise_xor(a, b, result); break;
    case 6: cv::min(a, b, result); break;
    case 7: cv::max(a, b, result); break;
    case 8: cv::addWeighted(a, 0.5, b, 0.5, 0, result); break; // 平均
    default: result = a.clone(); break;
    }

    setOutputImage(context, std::make_shared<CvImage>(result.clone()));
    setResultData("outputWidth", result.cols);
    setResultData("outputHeight", result.rows);
    setResultData("operation", op);
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ImageOperation, "图像运算", VisionInspector::ToolCategory::ImageProcess)
