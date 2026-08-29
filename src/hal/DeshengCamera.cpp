#include "DeshengCamera.h"
#include "../utils/Logger.h"

namespace VisionInspector {

DeshengCamera::DeshengCamera(QObject* parent)
    : ICameraDriver(parent)
{
    m_grabTimer = new QTimer(this);
    connect(m_grabTimer, &QTimer::timeout, this, &DeshengCamera::onGrabFrame);
}

DeshengCamera::~DeshengCamera()
{
    closeCamera();
}

#ifdef VI_HAS_DVP2

QList<CameraInfo> DeshengCamera::enumerateCameras()
{
    QList<CameraInfo> cameras;
    dvpUint32 count = 0;
    dvpRefresh(&count);
    VI_LOG_INFO(QString("dvpRefresh returned count: %1").arg(count));

    for (dvpUint32 i = 0; i < count; i++)
    {
        dvpCameraInfo info;
        if (dvpEnum(i, &info) == DVP_STATUS_OK)
        {
            CameraInfo camInfo;
            camInfo.id = QString::number(i);
            camInfo.vendor = QString::fromUtf8(info.Vendor);
            camInfo.model = QString::fromUtf8(info.Model);
            camInfo.serialNumber = QString::fromUtf8(info.SerialNumber);
            camInfo.ipAddress = QString::fromUtf8(info.LinkName);
            cameras.append(camInfo);
            VI_LOG_INFO(QString("Found camera: %1 %2 (ID: %3)")
                       .arg(camInfo.vendor).arg(camInfo.model).arg(camInfo.id));
        }
    }
    return cameras;
}

bool DeshengCamera::openCamera(const QString& cameraId)
{
    if (m_isOpen) return true;

    dvpUint32 index = cameraId.toUInt();
    dvpHandle handle = 0;
    dvpStatus status = dvpOpen(index, dvpOpenMode(OPEN_NORMAL), &handle);

    if (status == DVP_STATUS_OK)
    {
        m_handle = handle;
        m_currentCamera.id = cameraId;

        dvpCameraInfo camInfo;
        if (dvpGetCameraInfo(m_handle, &camInfo) == DVP_STATUS_OK)
        {
            m_currentCamera.vendor = QString::fromUtf8(camInfo.Vendor);
            m_currentCamera.model = QString::fromUtf8(camInfo.Model);
            m_currentCamera.serialNumber = QString::fromUtf8(camInfo.SerialNumber);
            m_currentCamera.ipAddress = QString::fromUtf8(camInfo.LinkName);
        }

        // Disable trigger mode
        dvpSetTriggerState(m_handle, false);
        VI_LOG_INFO("Trigger mode disabled");

        m_currentCamera.isConnected = true;
        m_isOpen = true;
        VI_LOG_INFO(QString("Opened camera %1, handle: %2").arg(cameraId).arg((uintptr_t)m_handle));
        return true;
    }
    VI_LOG_ERROR(QString("dvpOpen failed, status: %1").arg((int)status));
    return false;
}

void DeshengCamera::closeCamera()
{
    if (!m_isOpen) return;
    stopAcquisition();
    dvpClose(m_handle);
    m_handle = 0;
    m_isOpen = false;
    m_currentCamera.isConnected = false;
    VI_LOG_INFO("Camera closed");
}

CameraParams DeshengCamera::getParams() const
{
    CameraParams params;
    if (!m_isOpen) return params;
    double exposure;
    dvpGetExposure(m_handle, &exposure);
    params.exposureTime = exposure;
    float gain;
    dvpGetAnalogGain(m_handle, &gain);
    params.gain = gain;
    return params;
}

bool DeshengCamera::setParams(const CameraParams& params)
{
    if (!m_isOpen) return false;
    dvpSetExposure(m_handle, params.exposureTime);
    dvpSetAnalogGain(m_handle, params.gain);
    return true;
}

bool DeshengCamera::startAcquisition()
{
    if (!m_isOpen || m_isAcquiring) return false;

    dvpStatus status = dvpStart(m_handle);
    if (status == DVP_STATUS_OK)
    {
        m_isAcquiring = true;
        m_grabTimer->start(100); // 100ms = ~10fps (slower for stability)
        VI_LOG_INFO("Acquisition started, timer interval: 100ms");
        return true;
    }
    VI_LOG_ERROR(QString("dvpStart failed, status: %1").arg((int)status));
    return false;
}

bool DeshengCamera::stopAcquisition()
{
    if (!m_isAcquiring) return true;
    m_grabTimer->stop();
    dvpStop(m_handle);
    m_isAcquiring = false;
    return true;
}

CvImage DeshengCamera::grabFrame(int timeoutMs)
{
    if (!m_isOpen)
    {
        VI_LOG_ERROR("grabFrame: camera not open");
        return CvImage();
    }

    dvpFrame frame;
    void* buffer = nullptr;
    dvpStatus status = dvpGetFrame(m_handle, &frame, &buffer, timeoutMs);

    if (status != DVP_STATUS_OK)
    {
        VI_LOG_ERROR(QString("dvpGetFrame failed, status: %1").arg((int)status));
        return CvImage();
    }

    VI_LOG_INFO(QString("Frame: %1x%2, format: %3, buffer: %4")
               .arg(frame.iWidth).arg(frame.iHeight)
               .arg((int)frame.format)
               .arg((uintptr_t)buffer));

#ifdef VI_HAS_OPENCV
    // OpenCV模式：直接创建cv::Mat
    cv::Mat mat;
    if (frame.format == FORMAT_MONO)
    {
        mat = cv::Mat(frame.iHeight, frame.iWidth, CV_8UC1, buffer).clone();
    }
    else if (frame.format == FORMAT_BGR24)
    {
        mat = cv::Mat(frame.iHeight, frame.iWidth, CV_8UC3, buffer).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    }
    else if (frame.format == FORMAT_RGB24)
    {
        mat = cv::Mat(frame.iHeight, frame.iWidth, CV_8UC3, buffer).clone();
    }
    else
    {
        VI_LOG_WARN(QString("Unknown format: %1, trying grayscale").arg((int)frame.format));
        mat = cv::Mat(frame.iHeight, frame.iWidth, CV_8UC1, buffer).clone();
    }

    VI_LOG_INFO(QString("Image created: %1x%2").arg(mat.cols).arg(mat.rows));

    if (!mat.empty())
        return mat;

    return CvImage();
#else
    // QImage模式
    QImage image;

    if (frame.format == FORMAT_MONO)
    {
        image = QImage((uchar*)buffer, frame.iWidth, frame.iHeight,
                       static_cast<int>(frame.iWidth), QImage::Format_Grayscale8);
    }
    else if (frame.format == FORMAT_BGR24)
    {
        image = QImage((uchar*)buffer, frame.iWidth, frame.iHeight,
                       static_cast<int>(frame.iWidth * 3), QImage::Format_RGB888);
        image = image.rgbSwapped();
    }
    else if (frame.format == FORMAT_RGB24)
    {
        image = QImage((uchar*)buffer, frame.iWidth, frame.iHeight,
                       static_cast<int>(frame.iWidth * 3), QImage::Format_RGB888);
    }
    else
    {
        VI_LOG_WARN(QString("Unknown format: %1, trying grayscale").arg((int)frame.format));
        image = QImage((uchar*)buffer, frame.iWidth, frame.iHeight,
                       static_cast<int>(frame.iWidth), QImage::Format_Grayscale8);
    }

    VI_LOG_INFO(QString("Image created: %1, isNull: %2").arg(image.width()).arg(image.isNull()));

    if (!image.isNull())
        return image.copy();

    return CvImage();
#endif
}

QStringList DeshengCamera::supportedPixelFormats() const
{
    return {"Mono8", "Mono10", "Mono12", "BGR24", "RGB24"};
}

#else // VI_HAS_DVP2 not defined — stub implementations

QList<CameraInfo> DeshengCamera::enumerateCameras()
{
    VI_LOG_WARN("DVP2 SDK not available, camera enumeration disabled");
    return {};
}

bool DeshengCamera::openCamera(const QString& cameraId)
{
    Q_UNUSED(cameraId);
    VI_LOG_ERROR("DVP2 SDK not available, cannot open camera");
    return false;
}

void DeshengCamera::closeCamera()
{
}

CameraParams DeshengCamera::getParams() const
{
    return {};
}

bool DeshengCamera::setParams(const CameraParams& params)
{
    Q_UNUSED(params);
    return false;
}

bool DeshengCamera::startAcquisition()
{
    VI_LOG_ERROR("DVP2 SDK not available, cannot start acquisition");
    return false;
}

bool DeshengCamera::stopAcquisition()
{
    return true;
}

CvImage DeshengCamera::grabFrame(int timeoutMs)
{
    Q_UNUSED(timeoutMs);
    return {};
}

QStringList DeshengCamera::supportedPixelFormats() const
{
    return {};
}

#endif // VI_HAS_DVP2

void DeshengCamera::onGrabFrame()
{
#ifdef VI_HAS_DVP2
    if (!m_isAcquiring) return;

    CvImage frame = grabFrame(200);
#ifdef VI_HAS_OPENCV
    if (!frame.empty())
#else
    if (!frame.isNull())
#endif
    {
        m_currentImage = frame;
#ifdef VI_HAS_OPENCV
        VI_LOG_INFO(QString("Emitting image: %1x%2").arg(frame.cols).arg(frame.rows));
#else
        VI_LOG_INFO(QString("Emitting image: %1x%2").arg(frame.width()).arg(frame.height()));
#endif
        emit imageReceived(frame);
    }
#else
    Q_UNUSED(this);
#endif
}

} // namespace VisionInspector
