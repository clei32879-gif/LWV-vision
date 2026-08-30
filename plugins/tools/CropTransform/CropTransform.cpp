/**
 * @file CropTransform.cpp
 * @brief 裁剪变换工具 (对标 CKVision 裁剪变换, P1-11 补齐)
 *
 * 先按 ROI 裁剪输入图, 再做几何变换并输出:
 * - 镜像: 水平/垂直
 * - 旋转: 90/180/270 度 (无损), 或任意角度 (以中心为原点)
 * - 缩放: 比例倍数 (双线性)
 * - 平移: X/Y 像素 (黑色填充)
 *
 * 输出: 变换后图像写入 Current + 输出尺寸
 */
#include "CropTransform.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif

namespace VisionInspector {

PropertyDefList CropTransform::propertyDefs() const {
    return {
        // ROI 裁剪
        PropertyDef::enumProp("roiType", "ROI类型", {"无", "矩形"}, 1),
        PropertyDef::doubleProp("roiCenterX", "ROI中心X", 70, 0, 10000),
        PropertyDef::doubleProp("roiCenterY", "ROI中心Y", 70, 0, 10000),
        PropertyDef::doubleProp("roiWidth", "ROI宽度", 100, 1, 10000),
        PropertyDef::doubleProp("roiHeight", "ROI高度", 100, 1, 10000),
        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),
        // 变换
        PropertyDef::enumProp("transform", "变换功能", {"无", "镜像", "旋转", "缩放", "平移"}, 0),
        PropertyDef::boolProp("horizontalMirror", "水平镜像", false),
        PropertyDef::boolProp("verticalMirror", "垂直镜像", false),
        PropertyDef::enumProp("rotateDir", "旋转方向", {"顺时针90°", "180°", "逆时针90°", "任意角度"}, 0),
        PropertyDef::doubleProp("angle", "旋转角度(度)", 45, -360, 360),
        PropertyDef::doubleProp("scale", "缩放比例", 1.0, 0.05, 20.0),
        PropertyDef::doubleProp("translateX", "平移X", 0, -1000, 1000),
        PropertyDef::doubleProp("translateY", "平移Y", 0, -1000, 1000),
    };
}

bool CropTransform::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    // 克隆输入: 避免就地变换污染源图 (浅拷贝共享缓冲)
    cv::Mat img = input->clone();
    if (img.channels() > 1) cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);   // 统一灰度便于测试断言

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
        const int w = std::min((int)rr.width(), img.cols - x);
        const int h = std::min((int)rr.height(), img.rows - y);
        if (w > 0 && h > 0) img = img(cv::Rect(x, y, w, h)).clone();
    }

    const int tf = propertyValue("transform").toInt();
    if (tf == 1) {   // 镜像
        int flipCode = 0;
        const bool hMir = propertyValue("horizontalMirror").toBool();
        const bool vMir = propertyValue("verticalMirror").toBool();
        if (hMir && vMir) flipCode = -1;
        else if (vMir) flipCode = 0;          // 垂直: 绕 x 轴
        else if (hMir) flipCode = 1;          // 水平: 绕 y 轴
        cv::flip(img, img, flipCode);
    } else if (tf == 2) {   // 旋转
        const int rd = propertyValue("rotateDir").toInt();
        if (rd == 0) cv::rotate(img, img, cv::ROTATE_90_CLOCKWISE);
        else if (rd == 1) cv::rotate(img, img, cv::ROTATE_180);
        else if (rd == 2) cv::rotate(img, img, cv::ROTATE_90_COUNTERCLOCKWISE);
        else {   // 任意角度
            const double ang = propertyValue("angle").toDouble();
            cv::Point2f center(img.cols / 2.0f, img.rows / 2.0f);
            cv::Mat rot = cv::getRotationMatrix2D(center, ang, 1.0);
            cv::warpAffine(img, img, rot, img.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar::all(0));
        }
    } else if (tf == 3) {   // 缩放
        const double s = propertyValue("scale").toDouble();
        cv::resize(img, img, cv::Size(), s, s, cv::INTER_LINEAR);
    } else if (tf == 4) {   // 平移
        const double tx = propertyValue("translateX").toDouble();
        const double ty = propertyValue("translateY").toDouble();
        cv::Mat t = (cv::Mat_<double>(2, 3) << 1, 0, tx, 0, 1, ty);
        cv::warpAffine(img, img, t, img.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar::all(0));
    }

    setOutputImage(context, std::make_shared<CvImage>(img.clone()));
    setResultData("outputWidth", img.cols);
    setResultData("outputHeight", img.rows);
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CropTransform, "裁剪变换", VisionInspector::ToolCategory::ImageProcess)