/**
 * @file ImageUndistort.cpp
 * @brief 图像去畸变工具 (对标 CKVision 去畸变, §3.3 标定闭环下游)
 *
 * 使用相机内参 + 畸变系数对图像做去畸变校正。
 * 参数来源两种:
 *   1) 相机标定(Calibration 方法3)写入上下文的 calibration_camera_* (fx/fy/cx/cy/dist)
 *   2) 手动输入内参与畸变系数 (k1,k2,p1,p2,k3)
 *
 * 输出: 去畸变后的图像写入当前图像流, 供后续检测/测量工具使用;
 *       结果键: undistorted=true, source(0上下文/1手动), fx/fy/cx/cy, reprojError(若有)
 */
#include "ImageUndistort.h"
#include "../../../src/engine/ToolRegistry.h"
#include <opencv2/imgproc.hpp>
#include <QVariantList>

namespace VisionInspector {

PropertyDefList ImageUndistort::propertyDefs() const {
    return {
        PropertyDef::boolProp("useContextCalib", "使用标定结果", true),
        // 手动内参 (useContextCalib=false 时使用)
        PropertyDef::doubleProp("fx", "焦距fx", 1000.0, 1e-3, 1e9),
        PropertyDef::doubleProp("fy", "焦距fy", 1000.0, 1e-3, 1e9),
        PropertyDef::doubleProp("cx", "主点cx", 0.0, -1e6, 1e6),
        PropertyDef::doubleProp("cy", "主点cy", 0.0, -1e6, 1e6),
        // 手动畸变系数
        PropertyDef::doubleProp("k1", "径向k1", 0.0, -1e3, 1e3),
        PropertyDef::doubleProp("k2", "径向k2", 0.0, -1e3, 1e3),
        PropertyDef::doubleProp("p1", "切向p1", 0.0, -1e3, 1e3),
        PropertyDef::doubleProp("p2", "切向p2", 0.0, -1e3, 1e3),
        PropertyDef::doubleProp("k3", "径向k3", 0.0, -1e3, 1e3),
    };
}

bool ImageUndistort::execute(ToolContext& context) {
    CvImagePtr img = getInputImage(context);
    if (!img || img->empty()) {
        setResultData("error", "无输入图像");
        setStatus(ToolStatus::NG);
        return false;
    }

    // 1) 解析内参与畸变系数
    cv::Mat cameraMatrix(3, 3, CV_64F, cv::Scalar(0));
    cameraMatrix.at<double>(0, 0) = 1.0;
    cameraMatrix.at<double>(1, 1) = 1.0;
    cameraMatrix.at<double>(2, 2) = 1.0;
    cv::Mat distCoeffs;
    int source = 0; // 0上下文 1手动

    const bool useCtx = propertyValue("useContextCalib").toBool();
    if (useCtx && context.hasData("calibration_camera_fx")) {
        const double fx = context.getDouble("calibration_camera_fx", 1000.0);
        const double fy = context.getDouble("calibration_camera_fy", 1000.0);
        const double cx = context.getDouble("calibration_camera_cx", 0.0);
        const double cy = context.getDouble("calibration_camera_cy", 0.0);
        cameraMatrix.at<double>(0, 0) = fx;
        cameraMatrix.at<double>(1, 1) = fy;
        cameraMatrix.at<double>(0, 2) = cx;
        cameraMatrix.at<double>(1, 2) = cy;
        const QVariantList distList = context.getData("calibration_camera_dist").toList();
        if (!distList.isEmpty()) {
            distCoeffs = cv::Mat(1, (int)distList.size(), CV_64F);
            for (int i = 0; i < distList.size(); ++i)
                distCoeffs.at<double>(i) = distList.at(i).toDouble();
        }
    } else {
        source = 1;
        cameraMatrix.at<double>(0, 0) = propertyValue("fx").toDouble();
        cameraMatrix.at<double>(1, 1) = propertyValue("fy").toDouble();
        cameraMatrix.at<double>(0, 2) = propertyValue("cx").toDouble();
        cameraMatrix.at<double>(1, 2) = propertyValue("cy").toDouble();
        distCoeffs = (cv::Mat_<double>(1, 5) <<
                      propertyValue("k1").toDouble(),
                      propertyValue("k2").toDouble(),
                      propertyValue("p1").toDouble(),
                      propertyValue("p2").toDouble(),
                      propertyValue("k3").toDouble());
    }

    // 2) 构建去畸变映射并重采样
    cv::Mat map1, map2;
    cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), cameraMatrix,
                                img->size(), CV_32FC1, map1, map2);
    CvImagePtr out = std::make_shared<cv::Mat>();
    cv::remap(*img, *out, map1, map2, cv::INTER_LINEAR);

    setOutputImage(context, out);
    setResultData("undistorted", true);
    setResultData("source", source);
    setResultData("fx", cameraMatrix.at<double>(0, 0));
    setResultData("fy", cameraMatrix.at<double>(1, 1));
    setResultData("cx", cameraMatrix.at<double>(0, 2));
    setResultData("cy", cameraMatrix.at<double>(1, 2));
    if (context.hasData("calibration_reprojection_error"))
        setResultData("reprojError", context.getDouble("calibration_reprojection_error", 0.0));
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ImageUndistort, "图像去畸变", VisionInspector::ToolCategory::ImageProcess)
