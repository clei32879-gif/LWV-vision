#include "CaptureImage.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/hal/ICameraDriver.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#endif
#include <QDir>
#include <QFileInfoList>

namespace VisionInspector {

PropertyDefList CaptureImage::propertyDefs() const {
    return {
        PropertyDef::enumProp("source", "图像来源", {"从相机采集", "从文件加载", "图像列表"}, 0),
        PropertyDef::stringProp("cameraName", "相机别名", "", "采集"),
        PropertyDef::stringProp("imagePath", "图像路径", ""),
        PropertyDef::stringProp("imageDir", "图像目录", ""),
        PropertyDef::boolProp("autoNext", "自动换图", false),
        PropertyDef::intProp("timeoutMs", "采集超时(ms)", 3000, 100, 30000),
    };
}

bool CaptureImage::execute(ToolContext& context) {
    int source = propertyValue("source").toInt();

#ifdef VI_HAS_OPENCV
    if (source == 0) {
        // 从相机采集: 优先按相机别名取多相机表 (CCD1~CCD8), 空则用主相机
        ICameraDriver* cam = nullptr;
        const QString camName = propertyValue("cameraName").toString().trimmed();
        if (!camName.isEmpty()) {
            cam = context.namedCamera(camName);
            if (!cam) {
                setResultData("error", QString("未找到相机 \"%1\"").arg(camName));
                setResultData("status", QString("可用相机: %1 (相机管理里连接后自动注册)")
                                        .arg(context.namedCameraNames().join(", ")));
                setStatus(ToolStatus::NG);
                return false;
            }
        } else {
            cam = context.cameraDriver();
        }
        if (!cam) {
            setResultData("error", "未设置相机驱动");
            setResultData("status", "请先通过[相机管理]连接相机");
            setStatus(ToolStatus::NG);
            return false;
        }
        if (!cam->isOpen()) {
            setResultData("error", "相机未打开");
            setResultData("status", "请先通过菜单[相机→扫描相机]连接并打开相机");
            setStatus(ToolStatus::NG);
            return false;
        }
        if (!cam->isAcquiring()) {
            // 尝试开始采集
            if (!cam->startAcquisition()) {
                setResultData("error", "无法开始采集");
                setResultData("status", "相机采集启动失败");
                setStatus(ToolStatus::NG);
                return false;
            }
        }

        int timeout = propertyValue("timeoutMs").toInt();
        CvImage frame = cam->grabFrame(timeout);
        if (frame.empty()) {
            setResultData("error", "采集超时");
            setResultData("status", QString("等待%1ms未获取到图像").arg(timeout));
            setStatus(ToolStatus::NG);
            return false;
        }

        auto img = std::make_shared<CvImage>(frame.clone());
        context.setCurrentImage(img);

        CameraInfo info = cam->currentCamera();
        setResultData("width", frame.cols);
        setResultData("height", frame.rows);
        setResultData("camera", info.model);
        setResultData("source", "camera");
        setStatus(ToolStatus::OK);
        return true;

    } else if (source == 1) {
        // 从文件加载
        QString path = propertyValue("imagePath").toString();
        if (path.isEmpty()) {
            setResultData("error", "未设置图像路径");
            setResultData("status", "请在属性中设置图像路径或点击...按钮选择文件");
            setStatus(ToolStatus::NG);
            return false;
        }

        // 使用Qt加载图像（支持中文路径），然后转换为cv::Mat
        QImage qimg;
        if (!qimg.load(path)) {
            setResultData("error", QString("无法加载: %1").arg(path));
            setResultData("status", "文件不存在或格式不支持");
            setStatus(ToolStatus::NG);
            return false;
        }

        // 转换QImage为cv::Mat
        cv::Mat img = qImageToCvMat(qimg);
        if (img.empty()) {
            setResultData("error", "图像转换失败");
            setStatus(ToolStatus::NG);
            return false;
        }

        auto mat = std::make_shared<CvImage>(img.clone());
        context.setCurrentImage(mat);
        setResultData("width", img.cols);
        setResultData("height", img.rows);
        setResultData("loadedFile", path);
        setResultData("source", "file");
        setStatus(ToolStatus::OK);
        return true;

    } else {
        // 从图像列表加载（自动换图）
        QString dir = propertyValue("imageDir").toString();
        bool autoNext = propertyValue("autoNext").toBool();
        if (dir.isEmpty()) {
            setResultData("error", "未设置图像目录");
            setResultData("status", "请在属性中设置图像目录或点击...按钮选择文件夹");
            setStatus(ToolStatus::NG);
            return false;
        }

        QDir d(dir);
        QStringList filters;
        filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tif" << "*.tiff";
        QFileInfoList files = d.entryInfoList(filters, QDir::Files, QDir::Name);
        if (files.isEmpty()) {
            setResultData("error", "目录中无图像文件");
            setResultData("status", QString("目录: %1").arg(dir));
            setStatus(ToolStatus::NG);
            return false;
        }

        int idx = context.getInt("captureImageIndex", 0);
        if (idx >= files.size()) idx = 0;

        QString filePath = files[idx].absoluteFilePath();
        // 中文路径兼容: 用Qt加载再转cv::Mat (cv::imread在Windows下不支持中文路径)
        QImage qimg;
        if (!qimg.load(filePath)) {
            setResultData("error", QString("无法加载: %1").arg(filePath));
            setResultData("status", "文件已损坏或格式不支持");
            setStatus(ToolStatus::NG);
            return false;
        }
        cv::Mat img = qImageToCvMat(qimg);
        if (img.empty()) {
            setResultData("error", QString("图像转换失败: %1").arg(filePath));
            setStatus(ToolStatus::NG);
            return false;
        }

        auto mat = std::make_shared<CvImage>(img.clone());
        context.setCurrentImage(mat);
        setResultData("width", img.cols);
        setResultData("height", img.rows);
        setResultData("loadedFile", filePath);
        setResultData("fileIndex", idx);
        setResultData("totalFiles", files.size());
        setResultData("source", "list");

        if (autoNext) {
            context.setData("captureImageIndex", (idx + 1) % files.size());
        }

        setStatus(ToolStatus::OK);
        return true;
    }
#else
    setResultData("error", "需要OpenCV库");
    setStatus(ToolStatus::NG);
    return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CaptureImage, "采集图像", VisionInspector::ToolCategory::Camera)
