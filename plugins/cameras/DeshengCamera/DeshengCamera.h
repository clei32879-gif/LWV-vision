/**
 * @file DeshengCamera.h
 * @brief 度申(Do3think) DVP2 SDK 相机驱动
 *
 * 使用 DVP2 SDK (DVPCamera64.dll) 控制度申 GigE/USB 工业相机。
 * 通过 QLibrary 动态加载 DLL, 无需 MinGW .lib 文件。
 *
 * SDK 路径: E:/DVP2 SDK CN/
 * DLL 文件: DVPCamera64.dll, dvpir64.dll
 * 头文件:  DVPCamera.h, dvpParam.h
 */

#pragma once

#include "../../../src/hal/ICameraDriver.h"
#include <QLibrary>
#include <QVector>

// ============================================================
// DVP2 SDK 基础类型定义 (与 DVPCamera.h 保持一致)
// ============================================================

#ifndef DVP_TYPES_DEFINED
#define DVP_TYPES_DEFINED

typedef int                 dvpStatus;
typedef unsigned int        dvpUint32;
typedef unsigned short      dvpUint16;
typedef unsigned char       dvpUint8;
typedef void*               dvpHandle;
typedef char                dvpChar;
typedef bool                dvpBool;

#define DVP_STATUS_OK           0
#define DVP_STATUS_ERROR        -1
#define DVP_STATUS_TIMEOUT      -2
#define DVP_STATUS_BUSY         -3
#define DVP_STATUS_NOCAMERA     -4
#define DVP_MAX_CAMERA_COUNT    16

// 相机信息结构 (对应 dvpCameraInfo)
#pragma pack(push, 1)
struct dvpCameraInfo {
    dvpChar     FriendlyName[64];
    dvpChar     SerialNumber[64];
    dvpChar     Manufacturer[64];
    dvpUint16   VendorID;
    dvpUint16   ProductID;
    dvpChar     MacAddress[32];
    dvpChar     IPAddress[32];
    dvpChar     NetMask[32];
    dvpChar     Gateway[32];
    dvpUint32   Reseved[16];
};

// 图像信息结构 (对应 dvpFrame)
struct dvpFrame {
    dvpUint8*   pBuffer;
    dvpUint32   nBufferSize;
    dvpUint32   nWidth;
    dvpUint32   nHeight;
    dvpUint32   nPixelType;
    dvpUint32   nFrameID;
    dvpUint32   nTimestamp;
    dvpUint32   nPaddingX;
    dvpUint32   nPaddingY;
    dvpUint32   nReserved[4];
};
#pragma pack(pop)

// 像素格式
enum dvpPixelType {
    PIXEL_MONO8  = 0x01080001,
    PIXEL_MONO10 = 0x01100003,
    PIXEL_MONO12 = 0x01100005,
};

#endif // DVP_TYPES_DEFINED

namespace VisionInspector {

/**
 * DVP2 DLL 动态加载器
 *
 * 从 DVPCamera64.dll 加载所有 DVP2 API 函数指针。
 * 使用 QLibrary, 兼容 MinGW/MSVC。
 */
class DVP2Loader {
public:
    bool load();
    void unload();
    bool isLoaded() const { return m_loaded; }

    // ---- 函数指针类型定义 ----
    typedef dvpStatus (*dvpInit_t)();
    typedef dvpStatus (*dvpUninit_t)();
    typedef dvpStatus (*dvpRefresh_t)(dvpUint32* count);
    typedef dvpStatus (*dvpEnum_t)(dvpUint32 index, dvpCameraInfo* info);
    typedef dvpStatus (*dvpOpenByName_t)(const dvpChar* name, dvpHandle* handle);
    typedef dvpStatus (*dvpOpenBySN_t)(const dvpChar* sn, dvpHandle* handle);
    typedef dvpStatus (*dvpClose_t)(dvpHandle handle);
    typedef dvpStatus (*dvpStart_t)(dvpHandle handle);
    typedef dvpStatus (*dvpStop_t)(dvpHandle handle);
    typedef dvpStatus (*dvpGetFrame_t)(dvpHandle handle, dvpFrame* frame, dvpUint32 timeout);
    typedef dvpStatus (*dvpGetExposure_t)(dvpHandle handle, double* value);
    typedef dvpStatus (*dvpSetExposure_t)(dvpHandle handle, double value);
    typedef dvpStatus (*dvpGetGain_t)(dvpHandle handle, double* value);
    typedef dvpStatus (*dvpSetGain_t)(dvpHandle handle, double value);
    typedef dvpStatus (*dvpGetWidth_t)(dvpHandle handle, dvpUint32* value);
    typedef dvpStatus (*dvpSetWidth_t)(dvpHandle handle, dvpUint32 value);
    typedef dvpStatus (*dvpGetHeight_t)(dvpHandle handle, dvpUint32* value);
    typedef dvpStatus (*dvpSetHeight_t)(dvpHandle handle, dvpUint32 value);
    typedef dvpStatus (*dvpGetFrameRate_t)(dvpHandle handle, double* value);
    typedef dvpStatus (*dvpSetTriggerMode_t)(dvpHandle handle, dvpBool enable);
    typedef dvpStatus (*dvpTriggerOnce_t)(dvpHandle handle);

    // ---- 函数指针 ----
    dvpInit_t           dvpInit         = nullptr;
    dvpUninit_t         dvpUninit       = nullptr;
    dvpRefresh_t        dvpRefresh      = nullptr;
    dvpEnum_t           dvpEnum         = nullptr;
    dvpOpenByName_t     dvpOpenByName   = nullptr;
    dvpOpenBySN_t       dvpOpenBySN     = nullptr;
    dvpClose_t          dvpClose        = nullptr;
    dvpStart_t          dvpStart        = nullptr;
    dvpStop_t           dvpStop         = nullptr;
    dvpGetFrame_t       dvpGetFrame     = nullptr;
    dvpGetExposure_t    dvpGetExposure  = nullptr;
    dvpSetExposure_t    dvpSetExposure  = nullptr;
    dvpGetGain_t        dvpGetGain      = nullptr;
    dvpSetGain_t        dvpSetGain      = nullptr;
    dvpGetWidth_t       dvpGetWidth     = nullptr;
    dvpSetWidth_t       dvpSetWidth     = nullptr;
    dvpGetHeight_t      dvpGetHeight    = nullptr;
    dvpSetHeight_t      dvpSetHeight    = nullptr;
    dvpGetFrameRate_t   dvpGetFrameRate = nullptr;
    dvpSetTriggerMode_t dvpSetTriggerMode = nullptr;
    dvpTriggerOnce_t    dvpTriggerOnce  = nullptr;

private:
    QLibrary m_lib;
    bool m_loaded = false;

    template<typename T>
    bool resolve(const char* name, T& ptr);
};

/**
 * 度申相机驱动实现
 */
class DeshengCamera : public ICameraDriver {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.visioninspector.ICameraDriver")
    Q_INTERFACES(VisionInspector::ICameraDriver)

public:
    explicit DeshengCamera(QObject* parent = nullptr);
    ~DeshengCamera() override;

    // ICameraDriver 接口实现
    QString driverName() const override;
    QList<CameraInfo> enumerateCameras() override;
    bool openCamera(const QString& cameraId) override;
    void closeCamera() override;
    bool isOpen() const override;
    CameraInfo currentCamera() const override;
    CameraParams getParams() const override;
    bool setParams(const CameraParams& params) override;
    bool startAcquisition() override;
    bool stopAcquisition() override;
    bool isAcquiring() const override;
    CvImage grabFrame(int timeoutMs = 3000) override;
    bool triggerOnce() override;
    QStringList supportedPixelFormats() const override;

private:
    /** 将 dvpFrame 转换为 QImage (无OpenCV时) */
    QImage dvpFrameToQImage(const dvpFrame& frame) const;

    /** 将 dvpFrame 转换为 cv::Mat (有OpenCV时) */
#ifdef VI_HAS_OPENCV
    cv::Mat dvpFrameToCvMat(const dvpFrame& frame) const;
#endif

    /** 将 dvpCameraInfo 转换为 CameraInfo */
    CameraInfo toCameraInfo(const dvpCameraInfo& info, dvpUint32 index) const;

    /** 像素类型字符串 */
    QString pixelTypeString(dvpUint32 pixelType) const;

    DVP2Loader       m_api;
    dvpHandle        m_handle = nullptr;
    CameraInfo       m_currentInfo;
    CameraParams     m_currentParams;
    bool             m_acquiring = false;
    QVector<dvpCameraInfo> m_deviceList;
};

} // namespace VisionInspector
