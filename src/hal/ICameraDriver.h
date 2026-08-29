/**
 * @file ICameraDriver.h
 * @brief 相机驱动抽象接口
 *
 * 所有品牌的相机(度申/海康/Basler)都实现此接口。
 * 主程序通过此接口操作相机, 不关心具体品牌。
 *
 * GigE相机即插即用:
 *   1. enumerateCameras() 枚举网络上的所有GigE相机
 *   2. openCamera(id) 打开指定相机
 *   3. startAcquisition() 开始采集
 *   4. grabFrame() 获取一帧图像
 */

#pragma once

#define VI_TOOL_INTERFACE "org.visioninspector.ICameraDriver"

#include "../utils/Common.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QImage>
#include <QVariantMap>
#include <memory>
#include <functional>

namespace VisionInspector {

/**
 * 相机信息
 */
struct CameraInfo {
    QString id;
    QString vendor;
    QString model;
    QString ipAddress;
    QString serialNumber;
    bool isConnected = false;
};

/**
 * 相机参数
 */
struct CameraParams {
    double exposureTime = 5000.0;   // 曝光时间(微秒)
    double gain = 0.0;              // 增益(dB)
    int width = 1280;               // 图像宽度
    int height = 1024;              // 图像高度
    int offsetX = 0;                // X偏移
    int offsetY = 0;                // Y偏移
    bool triggerMode = false;       // 触发模式(false=连续采集)
    int triggerDelay = 0;           // 触发延迟(微秒)
    QString pixelFormat = "Mono8";  // 像素格式
    double frameRate = 30.0;        // 帧率
};

/**
 * 相机驱动抽象接口
 *
 * 实现示例:
 *   class AravisCameraDriver : public ICameraDriver { ... };  // Aravis通用GigE
 *   class HikrobotDriver : public ICameraDriver { ... };      // 海康MVS SDK
 *   class BaslerDriver : public ICameraDriver { ... };        // Basler Pylon SDK
 *   class DeshengDriver : public ICameraDriver { ... };       // 度申SDK
 */
class ICameraDriver : public QObject {
    Q_OBJECT

public:
    ICameraDriver(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ICameraDriver() = default;

    /**
     * 驱动名称 (如 "Aravis", "HikRobot MVS", "Basler Pylon")
     */
    virtual QString driverName() const = 0;

    /**
     * 枚举所有可用相机
     * @return 相机信息列表
     */
    virtual QList<CameraInfo> enumerateCameras() = 0;

    /**
     * 打开指定相机
     * @param cameraId 相机ID (来自enumerateCameras)
     * @return true=成功
     */
    virtual bool openCamera(const QString& cameraId) = 0;

    /**
     * 关闭当前相机
     */
    virtual void closeCamera() = 0;

    /**
     * 是否已打开
     */
    virtual bool isOpen() const = 0;

    /**
     * 获取当前相机信息
     */
    virtual CameraInfo currentCamera() const = 0;

    /**
     * 获取相机参数
     */
    virtual CameraParams getParams() const = 0;

    /**
     * 设置相机参数
     */
    virtual bool setParams(const CameraParams& params) = 0;

    /**
     * 开始采集
     */
    virtual bool startAcquisition() = 0;

    /**
     * 停止采集
     */
    virtual bool stopAcquisition() = 0;

    /**
     * 是否正在采集
     */
    virtual bool isAcquiring() const = 0;

    /**
     * 抓取一帧图像 (阻塞直到获取到图像或超时)
     * @param timeoutMs 超时(毫秒)
     * @return 图像, 空图像表示失败
     */
    virtual CvImage grabFrame(int timeoutMs = 3000) = 0;

    /**
     * 触发一次 (仅在触发模式下使用)
     */
    virtual bool triggerOnce() = 0;

    /**
     * 获取支持的像素格式列表
     */
    virtual QStringList supportedPixelFormats() const = 0;

signals:
    /**
     * 图像到达信号 (用于实时预览, 异步模式)
     */
    void imageReceived(const CvImage& image);

    /**
     * 相机连接状态变化
     */
    void connectionChanged(bool connected);

    /**
     * 错误信息
     */
    void errorOccurred(const QString& error);
};

} // namespace VisionInspector

Q_DECLARE_INTERFACE(VisionInspector::ICameraDriver, "org.visioninspector.ICameraDriver")
