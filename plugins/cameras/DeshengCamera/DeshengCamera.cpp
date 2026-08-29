/**
 * @file DeshengCamera.cpp
 * @brief 度申 DVP2 SDK 相机驱动实现
 */

#include "DeshengCamera.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace VisionInspector {

// ============================================================
// DVP2Loader 实现
// ============================================================

template<typename T>
bool DVP2Loader::resolve(const char* name, T& ptr) {
    ptr = reinterpret_cast<T>(m_lib.resolve(name));
    if (!ptr) {
        qWarning() << "[DeshengCamera] Failed to resolve:" << name;
        return false;
    }
    return true;
}

bool DVP2Loader::load() {
    if (m_loaded) return true;

    // 尝试多个可能的 DLL 路径
    QStringList searchPaths;

    // 1. 可执行文件所在目录
    searchPaths << QCoreApplication::applicationDirPath();

    // 2. DVP2 SDK 安装目录 (用户装 E 盘)
    searchPaths << "E:/DVP2 SDK CN/library/Visual C++/bin/x64";

    // 3. SDK 安装在系统盘
    searchPaths << "C:/Program Files/Do3think/DVP2 SDK/library/Visual C++/bin/x64";
    searchPaths << "C:/Program Files (x86)/Do3think/DVP2 SDK/library/Visual C++/bin/x64";

    // 4. 项目 thirdparty 目录
    searchPaths << QDir::currentPath() + "/thirdparty/desheng/bin";

    QString dllPath;
    for (const QString& dir : searchPaths) {
        QString candidate = QDir(dir).filePath("DVPCamera64");
        if (QFileInfo::exists(candidate + ".dll")) {
            dllPath = candidate;
            break;
        }
        // 也尝试绝对路径带上 .dll
        candidate = dir + "/DVPCamera64";
        if (QFileInfo::exists(candidate + ".dll")) {
            dllPath = candidate;
            break;
        }
    }

    if (dllPath.isEmpty()) {
        // 最后尝试系统 PATH
        m_lib.setFileName("DVPCamera64");
    } else {
        m_lib.setFileName(dllPath);
    }

    if (!m_lib.load()) {
        qWarning() << "[DeshengCamera] Failed to load DVPCamera64.dll:" << m_lib.errorString();
        qWarning() << "[DeshengCamera] Searched paths:" << searchPaths;
        return false;
    }

    qDebug() << "[DeshengCamera] Loaded:" << m_lib.fileName();

    // 解析所有 DVP2 API 函数
    // 函数名可能有不同的命名方案, 逐一尝试
    bool allOk = true;

    // dvpInitialize / dvpInit
    if (!resolve("dvpInitialize", dvpInit))
        allOk &= resolve("dvpInit", dvpInit);

    // dvpUninitialize / dvpUninit
    if (!resolve("dvpUninitialize", dvpUninit))
        allOk &= resolve("dvpUninit", dvpUninit);

    allOk &= resolve("dvpRefresh", dvpRefresh);
    allOk &= resolve("dvpEnum", dvpEnum);
    allOk &= resolve("dvpOpenByName", dvpOpenByName);
    allOk &= resolve("dvpOpenBySN", dvpOpenBySN);
    allOk &= resolve("dvpClose", dvpClose);
    allOk &= resolve("dvpStart", dvpStart);
    allOk &= resolve("dvpStop", dvpStop);

    // dvpGetFrame / dvpGetImage
    if (!resolve("dvpGetFrame", dvpGetFrame))
        allOk &= resolve("dvpGetImage", dvpGetFrame);

    allOk &= resolve("dvpGetExposure", dvpGetExposure);
    allOk &= resolve("dvpSetExposure", dvpSetExposure);
    allOk &= resolve("dvpGetGain", dvpGetGain);
    allOk &= resolve("dvpSetGain", dvpSetGain);
    allOk &= resolve("dvpGetWidth", dvpGetWidth);
    allOk &= resolve("dvpSetWidth", dvpSetWidth);
    allOk &= resolve("dvpGetHeight", dvpGetHeight);
    allOk &= resolve("dvpSetHeight", dvpSetHeight);

    // dvpGetFrameRate / dvpGetAcquisitionFrameRate
    if (!resolve("dvpGetFrameRate", dvpGetFrameRate))
        allOk &= resolve("dvpGetAcquisitionFrameRate", dvpGetFrameRate);

    allOk &= resolve("dvpSetTriggerMode", dvpSetTriggerMode);
    allOk &= resolve("dvpTriggerOnce", dvpTriggerOnce);

    m_loaded = true;

    if (!allOk) {
        qWarning() << "[DeshengCamera] Some DVP2 API functions could not be resolved.";
    }

    qDebug() << "[DeshengCamera] DVP2 API loaded successfully.";
    return true;
}

void DVP2Loader::unload() {
    if (m_loaded) {
        m_lib.unload();
        m_loaded = false;
    }
    // 清空所有函数指针
    dvpInit    = nullptr;
    dvpUninit  = nullptr;
    dvpRefresh = nullptr;
    dvpEnum    = nullptr;
    dvpOpenByName = nullptr;
    dvpOpenBySN   = nullptr;
    dvpClose   = nullptr;
    dvpStart   = nullptr;
    dvpStop    = nullptr;
    dvpGetFrame = nullptr;
    dvpGetExposure = nullptr;
    dvpSetExposure = nullptr;
    dvpGetGain  = nullptr;
    dvpSetGain  = nullptr;
    dvpGetWidth = nullptr;
    dvpSetWidth = nullptr;
    dvpGetHeight = nullptr;
    dvpSetHeight = nullptr;
    dvpGetFrameRate = nullptr;
    dvpSetTriggerMode = nullptr;
    dvpTriggerOnce = nullptr;
}

// ============================================================
// DeshengCamera 实现
// ============================================================

DeshengCamera::DeshengCamera(QObject* parent)
    : ICameraDriver(parent)
{
}

DeshengCamera::~DeshengCamera() {
    if (isAcquiring()) stopAcquisition();
    if (isOpen()) closeCamera();
    m_api.unload();
}

QString DeshengCamera::driverName() const {
    return QStringLiteral("Desheng DVP2 (MGS130M-H2)");
}

QList<CameraInfo> DeshengCamera::enumerateCameras() {
    QList<CameraInfo> result;

    if (!m_api.isLoaded() && !m_api.load()) {
        emit errorOccurred(QStringLiteral("无法加载 DVPCamera64.dll"));
        return result;
    }

    // 初始化
    if (m_api.dvpInit && m_api.dvpInit() != DVP_STATUS_OK) {
        emit errorOccurred(QStringLiteral("DVP2 初始化失败"));
        return result;
    }

    // 刷新设备列表
    dvpUint32 count = 0;
    if (m_api.dvpRefresh) {
        m_api.dvpRefresh(&count);
    }

    if (count == 0) {
        return result;
    }

    m_deviceList.clear();
    m_deviceList.resize(count);

    // 枚举所有相机
    for (dvpUint32 i = 0; i < count && i < DVP_MAX_CAMERA_COUNT; ++i) {
        dvpCameraInfo info;
        memset(&info, 0, sizeof(info));

        dvpStatus status = DVP_STATUS_ERROR;
        if (m_api.dvpEnum) {
            status = m_api.dvpEnum(i, &info);
        }

        if (status == DVP_STATUS_OK) {
            m_deviceList[i] = info;
            result.append(toCameraInfo(info, i));
        }
    }

    return result;
}

bool DeshengCamera::openCamera(const QString& cameraId) {
    if (!m_api.isLoaded()) {
        emit errorOccurred(QStringLiteral("DVP2 SDK 未加载"));
        return false;
    }

    if (m_handle) {
        closeCamera();
    }

    // cameraId 格式: "index:0" 或 "sn:XXXX" 或 "name:XXXX"
    dvpStatus status = DVP_STATUS_ERROR;

    if (cameraId.startsWith("index:")) {
        dvpUint32 index = cameraId.mid(6).toUInt();
        if (index < (dvpUint32)m_deviceList.size() && m_api.dvpOpenByName) {
            status = m_api.dvpOpenByName(m_deviceList[index].FriendlyName, &m_handle);
        }
    } else if (cameraId.startsWith("sn:") && m_api.dvpOpenBySN) {
        QByteArray sn = cameraId.mid(3).toUtf8();
        status = m_api.dvpOpenBySN(sn.constData(), &m_handle);
    } else if (m_api.dvpOpenByName) {
        QByteArray name = cameraId.toUtf8();
        status = m_api.dvpOpenByName(name.constData(), &m_handle);
    }

    if (status != DVP_STATUS_OK || !m_handle) {
        emit errorOccurred(QStringLiteral("打开相机失败: %1").arg(cameraId));
        return false;
    }

    // 更新当前相机信息
    if (!m_deviceList.isEmpty()) {
        dvpUint32 index = cameraId.startsWith("index:") ?
            cameraId.mid(6).toUInt() : 0;
        m_currentInfo = toCameraInfo(m_deviceList[index], index);
    }

    // 读取当前参数
    m_currentParams = getParams();

    emit connectionChanged(true);
    return true;
}

void DeshengCamera::closeCamera() {
    if (m_handle && m_api.dvpClose) {
        m_api.dvpClose(m_handle);
        m_handle = nullptr;
    }
    m_currentInfo = CameraInfo();
    emit connectionChanged(false);
}

bool DeshengCamera::isOpen() const {
    return m_handle != nullptr;
}

CameraInfo DeshengCamera::currentCamera() const {
    return m_currentInfo;
}

CameraParams DeshengCamera::getParams() const {
    CameraParams params = m_currentParams;

    if (m_handle && m_api.isLoaded()) {
        // 从相机读取实际参数
        double exposure = 0;
        double gain = 0;
        dvpUint32 width = 0;
        dvpUint32 height = 0;
        double fps = 0;

        if (m_api.dvpGetExposure && m_api.dvpGetExposure(m_handle, &exposure) == DVP_STATUS_OK)
            params.exposureTime = exposure;
        if (m_api.dvpGetGain && m_api.dvpGetGain(m_handle, &gain) == DVP_STATUS_OK)
            params.gain = gain;
        if (m_api.dvpGetWidth && m_api.dvpGetWidth(m_handle, &width) == DVP_STATUS_OK)
            params.width = static_cast<int>(width);
        if (m_api.dvpGetHeight && m_api.dvpGetHeight(m_handle, &height) == DVP_STATUS_OK)
            params.height = static_cast<int>(height);
        if (m_api.dvpGetFrameRate && m_api.dvpGetFrameRate(m_handle, &fps) == DVP_STATUS_OK)
            params.frameRate = fps;
    }

    return params;
}

bool DeshengCamera::setParams(const CameraParams& params) {
    if (!m_handle || !m_api.isLoaded()) return false;

    bool ok = true;

    if (m_api.dvpSetExposure)
        ok &= (m_api.dvpSetExposure(m_handle, params.exposureTime) == DVP_STATUS_OK);
    if (m_api.dvpSetGain)
        ok &= (m_api.dvpSetGain(m_handle, params.gain) == DVP_STATUS_OK);
    if (m_api.dvpSetWidth)
        ok &= (m_api.dvpSetWidth(m_handle, static_cast<dvpUint32>(params.width)) == DVP_STATUS_OK);
    if (m_api.dvpSetHeight)
        ok &= (m_api.dvpSetHeight(m_handle, static_cast<dvpUint32>(params.height)) == DVP_STATUS_OK);

    if (m_api.dvpSetTriggerMode)
        ok &= (m_api.dvpSetTriggerMode(m_handle, params.triggerMode) == DVP_STATUS_OK);

    if (ok) {
        m_currentParams = params;
    }

    return ok;
}

bool DeshengCamera::startAcquisition() {
    if (!m_handle || !m_api.dvpStart) {
        emit errorOccurred(QStringLiteral("相机未打开或SDK未就绪"));
        return false;
    }

    dvpStatus status = m_api.dvpStart(m_handle);
    if (status != DVP_STATUS_OK) {
        emit errorOccurred(QStringLiteral("开始采集失败"));
        return false;
    }

    m_acquiring = true;
    return true;
}

bool DeshengCamera::stopAcquisition() {
    if (!m_handle || !m_api.dvpStop) return false;

    dvpStatus status = m_api.dvpStop(m_handle);
    m_acquiring = false;
    return (status == DVP_STATUS_OK);
}

bool DeshengCamera::isAcquiring() const {
    return m_acquiring;
}

CvImage DeshengCamera::grabFrame(int timeoutMs) {
    if (!m_handle || !m_api.dvpGetFrame) {
        return CvImage();
    }

    dvpFrame frame;
    memset(&frame, 0, sizeof(frame));

    dvpStatus status = m_api.dvpGetFrame(m_handle, &frame, static_cast<dvpUint32>(timeoutMs));

    if (status != DVP_STATUS_OK) {
        if (status != DVP_STATUS_TIMEOUT) {
            emit errorOccurred(QStringLiteral("抓取图像失败, 状态码: %1").arg(status));
        }
        return CvImage();
    }

#ifdef VI_HAS_OPENCV
    return dvpFrameToCvMat(frame);
#else
    return dvpFrameToQImage(frame);
#endif
}

bool DeshengCamera::triggerOnce() {
    if (!m_handle || !m_api.dvpTriggerOnce) return false;
    return m_api.dvpTriggerOnce(m_handle) == DVP_STATUS_OK;
}

QStringList DeshengCamera::supportedPixelFormats() const {
    return {
        QStringLiteral("Mono8"),
        QStringLiteral("Mono10"),
        QStringLiteral("Mono12"),
    };
}

// ============================================================
// 私有辅助方法
// ============================================================

QImage DeshengCamera::dvpFrameToQImage(const dvpFrame& frame) const {
    if (!frame.pBuffer || frame.nBufferSize == 0)
        return QImage();

    int w = static_cast<int>(frame.nWidth);
    int h = static_cast<int>(frame.nHeight);

    switch (frame.nPixelType) {
        case PIXEL_MONO8: {
            // 8位灰度
            QImage img(frame.pBuffer, w, h, w, QImage::Format_Grayscale8);
            return img.copy();  // 深拷贝
        }
        case PIXEL_MONO10:
        case PIXEL_MONO12: {
            // 10/12位 -> 转换为16位灰度, 再转为8位显示
            QImage img(w, h, QImage::Format_Grayscale8);
            dvpUint16* src = reinterpret_cast<dvpUint16*>(frame.pBuffer);
            for (int y = 0; y < h; ++y) {
                uchar* dstLine = img.scanLine(y);
                for (int x = 0; x < w; ++x) {
                    // 右移4位 (16-bit -> 8-bit)
                    dstLine[x] = static_cast<uchar>((src[y * w + x] >> 4) & 0xFF);
                }
            }
            return img;
        }
        default:
            // 尝试按 Mono8 处理
            QImage img(frame.pBuffer, w, h, w, QImage::Format_Grayscale8);
            return img.copy();
    }
}

#ifdef VI_HAS_OPENCV
cv::Mat DeshengCamera::dvpFrameToCvMat(const dvpFrame& frame) const {
    int w = static_cast<int>(frame.nWidth);
    int h = static_cast<int>(frame.nHeight);

    switch (frame.nPixelType) {
        case PIXEL_MONO8:
            return cv::Mat(h, w, CV_8UC1, frame.pBuffer, w).clone();
        case PIXEL_MONO10:
        case PIXEL_MONO12: {
            cv::Mat src(h, w, CV_16UC1, frame.pBuffer, w * 2);
            cv::Mat dst;
            src.convertTo(dst, CV_8UC1, 1.0 / 16.0);
            return dst;
        }
        default: {
            cv::Mat src(h, w, CV_8UC1, frame.pBuffer, w);
            return src.clone();
        }
    }
}
#endif

CameraInfo DeshengCamera::toCameraInfo(const dvpCameraInfo& info, dvpUint32 index) const {
    CameraInfo result;
    result.id = QString("index:%1").arg(index);
    result.vendor = QString::fromLatin1(info.Manufacturer, 64).trimmed();
    if (result.vendor.isEmpty()) result.vendor = "Do3think";

    result.model = QString::fromLatin1(info.FriendlyName, 64).trimmed();
    result.serialNumber = QString::fromLatin1(info.SerialNumber, 64).trimmed();
    result.ipAddress = QString::fromLatin1(info.IPAddress, 32).trimmed();
    result.isConnected = true;
    return result;
}

QString DeshengCamera::pixelTypeString(dvpUint32 pixelType) const {
    switch (pixelType) {
        case PIXEL_MONO8:  return QStringLiteral("Mono8");
        case PIXEL_MONO10: return QStringLiteral("Mono10");
        case PIXEL_MONO12: return QStringLiteral("Mono12");
        default:           return QStringLiteral("Unknown");
    }
}

} // namespace VisionInspector
