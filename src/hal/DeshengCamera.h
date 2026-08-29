#pragma once

#include "../src/hal/ICameraDriver.h"

#ifdef VI_HAS_DVP2
#include "DVPCamera.h"
#endif

#include <QObject>
#include <QTimer>
#include <QImage>

namespace VisionInspector {

class DeshengCamera : public ICameraDriver {
    Q_OBJECT

public:
    explicit DeshengCamera(QObject* parent = nullptr);
    ~DeshengCamera();

    // ICameraDriver interface
    QString driverName() const override { return "Daheng DVP2"; }
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
    bool triggerOnce() override { return false; }
    QStringList supportedPixelFormats() const override;

private slots:
    void onGrabFrame();

private:
    bool m_isOpen = false;
    bool m_isAcquiring = false;
#ifdef VI_HAS_DVP2
    dvpHandle m_handle = 0;
#endif
    CameraInfo m_currentCamera;
    QTimer* m_grabTimer = nullptr;
    CvImage m_currentImage;
#ifdef VI_HAS_DVP2
    dvpFrame m_frame;
#endif
    void* m_buffer = nullptr;
};

} // namespace VisionInspector
