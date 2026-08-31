/**
 * @file BaslerCamera.cpp
 * @brief 巴斯勒相机驱动 — 动态加载 Pylon SDK DLL
 *
 * 运行时自动查找系统中已安装的 Pylon Runtime DLL 并加载。
 * 支持 Pylon 5/6/7 版本。不需要编译时链接 SDK。
 * 工控机装了 Pylon Viewer 就能用。
 */
#include "BaslerCamera.h"
#include "../utils/Logger.h"
#include <QDir>
#include <QStandardPaths>
#include <QLibrary>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <cstring>

#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

// Windows 动态加载
#ifdef _WIN32
#include <windows.h>
#define LOAD_DLL(path) LoadLibraryA(path)
#define GET_PROC(handle, name) GetProcAddress((HMODULE)handle, name)
#define FREE_DLL(handle) FreeLibrary((HMODULE)handle)
#else
#include <dlfcn.h>
#define LOAD_DLL(path) dlopen(path, RTLD_LAZY)
#define GET_PROC(handle, name) dlsym(handle, name)
#define FREE_DLL(handle) dlclose(handle)
#endif

namespace VisionInspector {

// Pylon 像素格式常量
static constexpr int PYLON_PIXEL_FORMAT_MONO8       = 0x01080001;
static constexpr int PYLON_PIXEL_FORMAT_MONO16      = 0x01100007;
static constexpr int PYLON_PIXEL_FORMAT_RGB8        = 0x02180014;
static constexpr int PYLON_PIXEL_FORMAT_BGR8        = 0x02180015;
static constexpr int PYLON_PIXEL_FORMAT_BAYER_BG8   = 0x0108000B;

// Pylon 访问模式
static constexpr int PYLON_ACCESS_MODE_EXCLUSIVE    = 1;
static constexpr int PYLON_ACCESS_MODE_CONTROL      = 2;

// Pylon 返回码
static constexpr int PYLON_OK                       = 0;

BaslerCamera::BaslerCamera(QObject* parent)
    : ICameraDriver(parent)
{
    m_grabTimer = new QTimer(this);
    connect(m_grabTimer, &QTimer::timeout, this, &BaslerCamera::onGrabFrame);
    // 不在构造函数加载DLL, 改为懒加载 (首次使用时加载)
}

BaslerCamera::~BaslerCamera()
{
    closeCamera();
    unloadPylonDll();
}

// ============================================================
// 动态加载 Pylon DLL
// ============================================================

bool BaslerCamera::loadPylonDll()
{
    // 尝试多个可能的 Pylon DLL 路径和版本
    QStringList dllNames = {
        // Pylon 8 (v7.5.0 实际版本)
        "PylonC_v8_0.dll",
        // Pylon 7
        "PylonC_v7_5.dll", "PylonC_v7_4.dll", "PylonC_v7_3.dll",
        "PylonC_v7_2.dll", "PylonC_v7_1.dll", "PylonC_v7_0.dll",
        // Pylon 6
        "PylonC_v6_3.dll", "PylonC_v6_2.dll", "PylonC_v6_1.dll", "PylonC_v6_0.dll",
        // 通用名
        "PylonC.dll", "pylon_c.dll",
    };

    // 搜索路径: 程序目录 → Pylon 安装目录
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath();

    QStringList pylonDirs = {
        "D:/Program Files/Basler/pylon 7",
        "C:/Program Files/Basler/pylon 7",
        "C:/Program Files/Basler/pylon 6",
        "D:/Program Files/Basler/pylon 6",
    };
    for (const auto& dir : pylonDirs) {
        searchPaths << dir + "/Runtime/x64";
        searchPaths << dir + "/Development/bin/x64";
        searchPaths << dir + "/bin";
    }

    // 尝试加载
    for (const auto& path : searchPaths) {
        for (const auto& dll : dllNames) {
            QString fullPath = path + "/" + dll;
            void* handle = LOAD_DLL(fullPath.toLocal8Bit().constData());
            if (handle) {
                // 验证关键函数是否存在
                auto init = (int(*)())GET_PROC(handle, "PylonInitialize");
                auto terminate = (void(*)())GET_PROC(handle, "PylonTerminate");
                if (init && terminate) {
                    m_dllHandle = handle;
                    m_pylonLoaded = true;

                    // 加载所有函数指针
                    m_api.Initialize = init;
                    m_api.Terminate = terminate;
                    m_api.IsInitialized = (int(*)())GET_PROC(handle, "PylonIsInitialized");
                    m_api.EnumerateDevices = (int(*)(size_t*))GET_PROC(handle, "PylonEnumerateDevices");
                    m_api.CreateDeviceByIndex = (int(*)(size_t, PYLON_DEVICE_HANDLE*))GET_PROC(handle, "PylonCreateDeviceByIndex");
                    m_api.DestroyDevice = (void(*)(PYLON_DEVICE_HANDLE))GET_PROC(handle, "PylonDestroyDevice");
                    m_api.DeviceGetDeviceInfo = (int(*)(PYLON_DEVICE_HANDLE, PYLON_DEVICE_INFO_HANDLE*))GET_PROC(handle, "PylonDeviceGetDeviceInfo");
                    m_api.DeviceInfoGetPropertyValue = (int(*)(PYLON_DEVICE_INFO_HANDLE, const char*, char*, size_t*))GET_PROC(handle, "PylonDeviceInfoGetPropertyValue");
                    m_api.DeviceOpen = (int(*)(PYLON_DEVICE_HANDLE, int))GET_PROC(handle, "PylonDeviceOpen");
                    m_api.DeviceClose = (void(*)(PYLON_DEVICE_HANDLE))GET_PROC(handle, "PylonDeviceClose");
                    m_api.DeviceIsOpen = (int(*)(PYLON_DEVICE_HANDLE))GET_PROC(handle, "PylonDeviceIsOpen");
                    m_api.DeviceGrabSingleFrame = (int(*)(PYLON_DEVICE_HANDLE, void*, size_t, size_t*, int*, int*, int*, int))GET_PROC(handle, "PylonDeviceGrabSingleFrame");
                    m_api.DeviceGetIntegerFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*, int64_t*))GET_PROC(handle, "PylonDeviceGetIntegerFeature");
                    m_api.DeviceSetIntegerFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*, int64_t))GET_PROC(handle, "PylonDeviceSetIntegerFeature");
                    m_api.DeviceGetFloatFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*, double*))GET_PROC(handle, "PylonDeviceGetFloatFeature");
                    m_api.DeviceSetFloatFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*, double))GET_PROC(handle, "PylonDeviceSetFloatFeature");
                    m_api.DeviceGetStringFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*, char*, size_t*))GET_PROC(handle, "PylonDeviceGetStringFeature");
                    m_api.DeviceSetStringFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*, const char*))GET_PROC(handle, "PylonDeviceSetStringFeature");
                    m_api.DeviceGetBooleanFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*, int*))GET_PROC(handle, "PylonDeviceGetBooleanFeature");
                    m_api.DeviceSetBooleanFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*, int))GET_PROC(handle, "PylonDeviceSetBooleanFeature");
                    m_api.DeviceExecuteCommandFeature = (int(*)(PYLON_DEVICE_HANDLE, const char*))GET_PROC(handle, "PylonDeviceExecuteCommandFeature");
                    m_api.DeviceIsCommandDone = (int(*)(PYLON_DEVICE_HANDLE, const char*, int*))GET_PROC(handle, "PylonDeviceIsCommandDone");

                    VI_LOG_INFO(QString("Pylon SDK 已加载: %1").arg(fullPath));
                    return true;
                }
                FREE_DLL(handle);
            }
        }
    }

    VI_LOG_WARN("Pylon SDK DLL 未找到, Basler 相机不可用");
    m_pylonLoaded = false;
    return false;
}

void BaslerCamera::unloadPylonDll()
{
    if (m_dllHandle) {
        FREE_DLL(m_dllHandle);
        m_dllHandle = nullptr;
        m_pylonLoaded = false;
        memset(&m_api, 0, sizeof(m_api));
    }
}

// ============================================================
// 辅助函数
// ============================================================

QString BaslerCamera::getDeviceStringProperty(PYLON_DEVICE_HANDLE handle, const char* name)
{
    if (!m_api.DeviceGetStringFeature) return {};
    char buf[256] = {0};
    size_t size = sizeof(buf) - 1;
    if (m_api.DeviceGetStringFeature(handle, name, buf, &size) == PYLON_OK) {
        return QString::fromLatin1(buf).trimmed();
    }
    return {};
}

int64_t BaslerCamera::getDeviceIntProperty(PYLON_DEVICE_HANDLE handle, const char* name)
{
    if (!m_api.DeviceGetIntegerFeature) return 0;
    int64_t val = 0;
    m_api.DeviceGetIntegerFeature(handle, name, &val);
    return val;
}

double BaslerCamera::getDeviceFloatProperty(PYLON_DEVICE_HANDLE handle, const char* name)
{
    if (!m_api.DeviceGetFloatFeature) return 0;
    double val = 0;
    m_api.DeviceGetFloatFeature(handle, name, &val);
    return val;
}

bool BaslerCamera::setDeviceIntProperty(PYLON_DEVICE_HANDLE handle, const char* name, int64_t value)
{
    if (!m_api.DeviceSetIntegerFeature) return false;
    return m_api.DeviceSetIntegerFeature(handle, name, value) == PYLON_OK;
}

bool BaslerCamera::setDeviceFloatProperty(PYLON_DEVICE_HANDLE handle, const char* name, double value)
{
    if (!m_api.DeviceSetFloatFeature) return false;
    return m_api.DeviceSetFloatFeature(handle, name, value) == PYLON_OK;
}

bool BaslerCamera::setDeviceBoolProperty(PYLON_DEVICE_HANDLE handle, const char* name, bool value)
{
    if (!m_api.DeviceSetBooleanFeature) return false;
    return m_api.DeviceSetBooleanFeature(handle, name, value ? 1 : 0) == PYLON_OK;
}

bool BaslerCamera::executeCommand(PYLON_DEVICE_HANDLE handle, const char* name)
{
    if (!m_api.DeviceExecuteCommandFeature) return false;
    return m_api.DeviceExecuteCommandFeature(handle, name) == PYLON_OK;
}

// ============================================================
// 设备枚举
// ============================================================

QList<CameraInfo> BaslerCamera::enumerateCameras()
{
    QList<CameraInfo> result;
    if (!m_pylonLoaded) {
        m_lastError = "Pylon SDK 未加载";
        return result;
    }

    // 初始化 Pylon (如果还没初始化)
    if (m_api.IsInitialized && !m_api.IsInitialized()) {
        m_api.Initialize();
    }

    // 枚举设备
    size_t numDevices = 0;
    if (m_api.EnumerateDevices(&numDevices) != PYLON_OK) {
        m_lastError = "Pylon 枚举设备失败";
        return result;
    }

    VI_LOG_INFO(QString("Pylon 发现 %1 台 Basler 相机").arg(numDevices));

    for (size_t i = 0; i < numDevices; i++) {
        PYLON_DEVICE_HANDLE handle = nullptr;
        if (m_api.CreateDeviceByIndex(i, &handle) != PYLON_OK || !handle) continue;

        CameraInfo cam;
        cam.vendor = getDeviceStringProperty(handle, "DeviceVendorName");
        cam.model = getDeviceStringProperty(handle, "DeviceModelName");
        cam.serialNumber = getDeviceStringProperty(handle, "DeviceSerialNumber");
        cam.id = cam.serialNumber;  // 用序列号做标识

        // 尝试获取 IP 地址
        cam.ipAddress = getDeviceStringProperty(handle, "GevCurrentIPAddress");
        if (cam.ipAddress.isEmpty()) {
            cam.ipAddress = getDeviceStringProperty(handle, "DeviceUserID");
        }

        result.append(cam);
        VI_LOG_INFO(QString("  Basler #%1: %2 %3 SN=%4 IP=%5")
            .arg(i).arg(cam.vendor, cam.model, cam.serialNumber, cam.ipAddress));

        m_api.DestroyDevice(handle);
    }

    return result;
}

// ============================================================
// 连接/断开
// ============================================================

bool BaslerCamera::openCamera(const QString& cameraId)
{
    if (m_isOpen) return true;
    if (!m_pylonLoaded) {
        m_lastError = "Pylon SDK 未加载, 请安装 Basler Pylon Viewer";
        return false;
    }

    // 初始化 Pylon
    if (m_api.IsInitialized && !m_api.IsInitialized()) {
        m_api.Initialize();
    }

    // 枚举设备, 找到匹配的
    size_t numDevices = 0;
    m_api.EnumerateDevices(&numDevices);

    for (size_t i = 0; i < numDevices; i++) {
        PYLON_DEVICE_HANDLE handle = nullptr;
        if (m_api.CreateDeviceByIndex(i, &handle) != PYLON_OK || !handle) continue;

        QString serial = getDeviceStringProperty(handle, "DeviceSerialNumber");
        QString model = getDeviceStringProperty(handle, "DeviceModelName");
        QString ip = getDeviceStringProperty(handle, "GevCurrentIPAddress");

        // 匹配: 序列号、IP、或用户ID
        if (serial == cameraId || ip == cameraId ||
            getDeviceStringProperty(handle, "DeviceUserID") == cameraId) {

            // 打开设备
            if (m_api.DeviceOpen(handle, PYLON_ACCESS_MODE_EXCLUSIVE) != PYLON_OK) {
                m_lastError = QString("无法打开相机 %1 (可能被其他程序占用)").arg(cameraId);
                m_api.DestroyDevice(handle);
                VI_LOG_ERROR(m_lastError);
                return false;
            }

            m_deviceHandle = handle;
            m_isOpen = true;

            // 读取图像参数
            m_imageWidth = (int)getDeviceIntProperty(handle, "Width");
            m_imageHeight = (int)getDeviceIntProperty(handle, "Height");

            m_currentCamera.id = cameraId;
            m_currentCamera.vendor = getDeviceStringProperty(handle, "DeviceVendorName");
            m_currentCamera.model = model;
            m_currentCamera.serialNumber = serial;
            m_currentCamera.ipAddress = ip;
            m_currentCamera.isConnected = true;

            // 关闭触发模式, 使用连续采集
            setDeviceBoolProperty(handle, "TriggerMode", false);

            VI_LOG_INFO(QString("Basler 相机已打开: %1 %2 SN=%3 %4x%5")
                .arg(m_currentCamera.vendor, m_currentCamera.model, serial)
                .arg(m_imageWidth).arg(m_imageHeight));
            emit connectionChanged(true);
            return true;
        }

        m_api.DestroyDevice(handle);
    }

    m_lastError = QString("未找到匹配的 Basler 相机: %1").arg(cameraId);
    VI_LOG_ERROR(m_lastError);
    return false;
}

void BaslerCamera::closeCamera()
{
    if (!m_isOpen) return;

    stopAcquisition();

    if (m_deviceHandle) {
        m_api.DeviceClose(m_deviceHandle);
        m_api.DestroyDevice(m_deviceHandle);
        m_deviceHandle = nullptr;
    }

    m_isOpen = false;
    m_currentCamera.isConnected = false;
    VI_LOG_INFO("Basler 相机已关闭");
    emit connectionChanged(false);
}

// ============================================================
// 参数
// ============================================================

CameraParams BaslerCamera::getParams() const
{
    CameraParams params;
    if (!m_isOpen || !m_deviceHandle) return params;

    params.width = m_imageWidth;
    params.height = m_imageHeight;
    params.exposureTime = (double)const_cast<BaslerCamera*>(this)->getDeviceIntProperty(m_deviceHandle, "ExposureTime");
    params.gain = (double)const_cast<BaslerCamera*>(this)->getDeviceFloatProperty(m_deviceHandle, "Gain");
    return params;
}

bool BaslerCamera::setParams(const CameraParams& params)
{
    if (!m_isOpen || !m_deviceHandle) return false;

    setDeviceFloatProperty(m_deviceHandle, "ExposureTime", params.exposureTime);
    setDeviceFloatProperty(m_deviceHandle, "Gain", params.gain);
    return true;
}

// ============================================================
// 采集
// ============================================================

bool BaslerCamera::startAcquisition()
{
    if (!m_isOpen || m_isAcquiring || !m_deviceHandle) return false;

    // 开始采集
    if (!executeCommand(m_deviceHandle, "AcquisitionStart")) {
        m_lastError = "AcquisitionStart 命令失败";
        VI_LOG_ERROR(m_lastError);
        return false;
    }

    m_isAcquiring = true;
    m_grabTimer->start(33); // ~30fps
    VI_LOG_INFO("Basler 采集已启动");
    return true;
}

bool BaslerCamera::stopAcquisition()
{
    if (!m_isAcquiring) return true;

    m_grabTimer->stop();

    if (m_deviceHandle) {
        executeCommand(m_deviceHandle, "AcquisitionStop");
    }

    m_isAcquiring = false;
    VI_LOG_INFO("Basler 采集已停止");
    return true;
}

CvImage BaslerCamera::grabFrame(int timeoutMs)
{
    if (!m_isOpen || !m_deviceHandle) return CvImage();

    // 使用 Pylon 的单帧抓取
    size_t bufferSize = m_imageWidth * m_imageHeight * 3 + 1024; // 足够大
    QByteArray buffer(bufferSize, '\0');
    size_t grabbedBytes = 0;
    int width = 0, height = 0, format = 0;

    if (m_api.DeviceGrabSingleFrame(m_deviceHandle, buffer.data(), bufferSize,
                                     &grabbedBytes, &width, &height, &format, timeoutMs) != PYLON_OK) {
        return m_lastFrame; // 返回上一帧
    }

    if (grabbedBytes == 0 || width == 0 || height == 0) return m_lastFrame;

    CvImage frame;
#ifdef VI_HAS_OPENCV
    if (format == PYLON_PIXEL_FORMAT_MONO8 || format == PYLON_PIXEL_FORMAT_BAYER_BG8) {
        frame = cv::Mat(height, width, CV_8UC1, buffer.data()).clone();
        if (format == PYLON_PIXEL_FORMAT_BAYER_BG8) {
            cv::cvtColor(frame, frame, cv::COLOR_BayerBG2BGR);
        }
    } else if (format == PYLON_PIXEL_FORMAT_RGB8) {
        frame = cv::Mat(height, width, CV_8UC3, buffer.data()).clone();
        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
    } else if (format == PYLON_PIXEL_FORMAT_BGR8) {
        frame = cv::Mat(height, width, CV_8UC3, buffer.data()).clone();
    } else {
        // 默认当 Mono8
        frame = cv::Mat(height, width, CV_8UC1, buffer.data()).clone();
    }
#else
    if (format == PYLON_PIXEL_FORMAT_MONO8) {
        frame = QImage((const uchar*)buffer.constData(), width, height, width, QImage::Format_Grayscale8).copy();
    } else {
        frame = QImage((const uchar*)buffer.constData(), width, height, width * 3, QImage::Format_RGB888).copy();
    }
#endif

    m_lastFrame = frame;
    return frame;
}

bool BaslerCamera::triggerOnce()
{
    if (!m_isOpen || !m_deviceHandle) return false;
    return executeCommand(m_deviceHandle, "TriggerSoftware");
}

QStringList BaslerCamera::supportedPixelFormats() const
{
    return {"Mono8", "Mono16", "RGB8", "BGR8", "BayerBG8"};
}

void BaslerCamera::onGrabFrame()
{
    if (!m_isAcquiring || m_grabbing) return;
    m_grabbing = true;

    CvImage frame = grabFrame(100);
#ifdef VI_HAS_OPENCV
    if (!frame.empty())
#else
    if (!frame.isNull())
#endif
    {
        emit imageReceived(frame);
    }

    m_grabbing = false;
}

} // namespace VisionInspector
