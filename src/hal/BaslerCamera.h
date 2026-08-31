/**
 * @file BaslerCamera.h
 * @brief 巴斯勒相机驱动 — 动态加载 Pylon SDK DLL
 *
 * 不需要编译时链接 Pylon SDK。
 * 运行时自动查找并加载系统中已安装的 Pylon Runtime DLL。
 * 工控机装了 Pylon Viewer 就能用。
 */
#pragma once
#include "ICameraDriver.h"
#include <QTimer>
#include <QThread>
#include <atomic>

// Pylon C API 函数指针类型定义
// 基于 PylonC.h 标准接口
typedef void* PYLON_DEVICE_HANDLE;
typedef void* PYLON_STREAMGRABBER_HANDLE;
typedef void* PYLON_BUFFER_HANDLE;
typedef int PYLON_DEVICE_INFO_HANDLE;

// Pylon 枚举结果
struct PylonDeviceInfo {
    char fullName[256];
    char friendlyName[256];
    char serialNumber[64];
    char vendorName[64];
    char modelName[64];
    char deviceClass[32];
    char ipAddress[64];
};

namespace VisionInspector {

class BaslerCamera : public ICameraDriver {
    Q_OBJECT
    Q_INTERFACES(VisionInspector::ICameraDriver)
public:
    explicit BaslerCamera(QObject* parent = nullptr);
    ~BaslerCamera() override;

    QString driverName() const override { return QStringLiteral("Basler Pylon"); }
    QList<CameraInfo> enumerateCameras() override;
    bool openCamera(const QString& cameraId) override;
    void closeCamera() override;
    bool isOpen() const override { return m_isOpen; }
    CameraInfo currentCamera() const override { return m_currentCamera; }
    CameraParams getParams() const override;
    bool setParams(const CameraParams& params) override;
    bool startAcquisition() override;
    bool stopAcquisition() override;
    bool isAcquiring() const override { return m_isAcquiring; }
    CvImage grabFrame(int timeoutMs = 3000) override;
    bool triggerOnce() override;
    QStringList supportedPixelFormats() const override;

    QString lastError() const { return m_lastError; }

    /// 是否成功加载了 Pylon DLL
    bool isPylonLoaded() const { return m_pylonLoaded; }

private:
    bool loadPylonDll();
    void unloadPylonDll();

    // Pylon C API 函数指针
    struct PylonAPI {
        // 初始化/终止
        int (*Initialize)() = nullptr;
        void (*Terminate)() = nullptr;
        int (*IsInitialized)() = nullptr;

        // 设备枚举
        int (*EnumerateDevices)(size_t* numDevices) = nullptr;
        int (*CreateDeviceByIndex)(size_t index, PYLON_DEVICE_HANDLE* handle) = nullptr;
        void (*DestroyDevice)(PYLON_DEVICE_HANDLE handle) = nullptr;

        // 设备信息
        int (*DeviceGetDeviceInfo)(PYLON_DEVICE_HANDLE handle, PYLON_DEVICE_INFO_HANDLE* infoHandle) = nullptr;
        int (*DeviceInfoGetPropertyValue)(PYLON_DEVICE_INFO_HANDLE infoHandle, const char* name, char* value, size_t* size) = nullptr;

        // 设备操作
        int (*DeviceOpen)(PYLON_DEVICE_HANDLE handle, int accessMode) = nullptr;
        void (*DeviceClose)(PYLON_DEVICE_HANDLE handle) = nullptr;
        int (*DeviceIsOpen)(PYLON_DEVICE_HANDLE handle) = nullptr;

        // 图像采集
        int (*DeviceGrabSingleFrame)(PYLON_DEVICE_HANDLE handle, void* buffer, size_t bufferSize,
                                      size_t* grabbedBytes, int* width, int* height, int* format, int timeoutMs) = nullptr;

        // 整数特征
        int (*DeviceGetIntegerFeature)(PYLON_DEVICE_HANDLE handle, const char* name, int64_t* value) = nullptr;
        int (*DeviceSetIntegerFeature)(PYLON_DEVICE_HANDLE handle, const char* name, int64_t value) = nullptr;

        // 浮点特征
        int (*DeviceGetFloatFeature)(PYLON_DEVICE_HANDLE handle, const char* name, double* value) = nullptr;
        int (*DeviceSetFloatFeature)(PYLON_DEVICE_HANDLE handle, const char* name, double value) = nullptr;

        // 字符串特征
        int (*DeviceGetStringFeature)(PYLON_DEVICE_HANDLE handle, const char* name, char* value, size_t* size) = nullptr;
        int (*DeviceSetStringFeature)(PYLON_DEVICE_HANDLE handle, const char* name, const char* value) = nullptr;

        // 布尔特征
        int (*DeviceGetBooleanFeature)(PYLON_DEVICE_HANDLE handle, const char* name, int* value) = nullptr;
        int (*DeviceSetBooleanFeature)(PYLON_DEVICE_HANDLE handle, const char* name, int value) = nullptr;

        // 命令执行
        int (*DeviceExecuteCommandFeature)(PYLON_DEVICE_HANDLE handle, const char* name) = nullptr;
        int (*DeviceIsCommandDone)(PYLON_DEVICE_HANDLE handle, const char* name, int* done) = nullptr;
    };

    PylonAPI m_api;
    bool m_pylonLoaded = false;
    void* m_dllHandle = nullptr;

    // 相机状态
    PYLON_DEVICE_HANDLE m_deviceHandle = nullptr;
    bool m_isOpen = false;
    bool m_isAcquiring = false;
    CameraInfo m_currentCamera;
    QString m_lastError;

    // 采集
    QTimer* m_grabTimer = nullptr;
    CvImage m_lastFrame;
    std::atomic<bool> m_grabbing{false};

    // 图像参数
    int m_imageWidth = 0;
    int m_imageHeight = 0;

    // 辅助函数
    QString getDeviceStringProperty(PYLON_DEVICE_HANDLE handle, const char* name);
    int64_t getDeviceIntProperty(PYLON_DEVICE_HANDLE handle, const char* name);
    double getDeviceFloatProperty(PYLON_DEVICE_HANDLE handle, const char* name);
    bool setDeviceIntProperty(PYLON_DEVICE_HANDLE handle, const char* name, int64_t value);
    bool setDeviceFloatProperty(PYLON_DEVICE_HANDLE handle, const char* name, double value);
    bool setDeviceBoolProperty(PYLON_DEVICE_HANDLE handle, const char* name, bool value);
    bool executeCommand(PYLON_DEVICE_HANDLE handle, const char* name);

private slots:
    void onGrabFrame();
};

} // namespace VisionInspector
