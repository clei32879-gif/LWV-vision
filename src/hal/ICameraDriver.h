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
#include <limits>
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
    // ---- 多类型相机适配 (阶段6: 面向 高像素/3D/线扫/红外 等品类) ----
    enum class SensorKind {
        Area,       // 面阵 2D (绝大多数场景)
        LineScan,   // 线阵 (行扫描, 传送带/大幅面)
        Stereo3D,   // 3D 双目/结构光 (高度图/点云)
        Laser3D,    // 激光轮廓仪 (3D 线扫, 高度剖面)
        Thermal,    // 红外热成像 (温度场, 短波/长波红外)
        SWIR,       // 短波红外 (水分/材质分选)
    };
    SensorKind sensorKind = SensorKind::Area;
    // 高像素支持: 用 double 容纳 1.5亿级 (10000x10000)
    // 分辨率信息由 getParams().width/height 承载 (int 上限 2^31 已够)
    QString sensorInfo;             // 传感器描述 (如 "IMX540 1.27亿" / "线阵8K" / "640x512 LWIR")
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

    // ---- 线阵相机专属 (LineScan) ----
    double lineRate = 20000.0;      // 行频 (Hz, 线阵替代 frameRate 的核心参数)
    int scanLineCount = 0;          // 每帧行数 (0=由height决定)
    QString encoderSource = "";     // 编码器触发源 (外触发时行频跟随传送带)

    // ---- 3D 专属 (Stereo3D/Laser3D) ----
    double zRangeMin = 0.0;         // Z 量程下限 (mm)
    double zRangeMax = 100.0;       // Z 量程上限 (mm)
    int profileCount = 0;           // 每帧轮廓线数 (激光3D)

    // ---- 红外专属 (Thermal/SWIR) ----
    double emissivity = 1.0;        // 发射率 (测温校准)
    bool temperatureDisplay = false; // 伪彩/测温模式
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

    // ============================================================
    // 多类型相机扩展 (默认实现: 面阵相机返回空/False, 各类驱动按需覆写)
    // ============================================================

    /** 传感器品类 (线扫/3D/红外驱动必须覆写) */
    virtual CameraInfo::SensorKind sensorKind() const { return CameraInfo::SensorKind::Area; }

    /** 线阵相机: 设置行频 (Hz); 面阵相机返回 false */
    virtual bool setLineRate(double /*hz*/) { return false; }

    /** 线阵相机: 编码器外触发模式 (行频跟随传送带); 面阵返回 false */
    virtual bool setEncoderTrigger(const QString& /*source*/) { return false; }

    /** 3D 相机: 获取当前帧的高度图 (CV_32F, mm); 非3D返回空 */
    virtual CvImage grabHeightMap(int /*timeoutMs = 3000*/) { return {}; }

    /** 红外相机: 获取当前帧中心区域温度 (℃); 非测温型返回 NaN */
    virtual double readCenterTemperature() { return std::numeric_limits<double>::quiet_NaN(); }

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
