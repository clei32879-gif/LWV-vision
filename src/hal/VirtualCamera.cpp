/**
 * @file VirtualCamera.cpp
 * @brief 虚拟相机驱动实现
 */

#include "VirtualCamera.h"
#include "../utils/Logger.h"

#include <QDir>
#include <QCoreApplication>
#include <QElapsedTimer>

#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>
#endif

namespace VisionInspector {

VirtualCamera::VirtualCamera(QObject* parent)
    : ICameraDriver(parent)
{
    m_playTimer = new QTimer(this);
    m_playTimer->setTimerType(Qt::PreciseTimer);
    connect(m_playTimer, &QTimer::timeout, this, &VirtualCamera::onPlaybackTick);

    // 默认回放目录: 可执行文件同级 testdata/virtual_camera
    m_imageDir = QCoreApplication::applicationDirPath() + "/testdata/virtual_camera";
}

VirtualCamera::~VirtualCamera()
{
    stopAcquisition();
    closeCamera();
}

QList<CameraInfo> VirtualCamera::enumerateCameras()
{
    CameraInfo info;
    info.id = QStringLiteral("virtual");
    info.vendor = QStringLiteral("LW Vision");
    info.model = QStringLiteral("虚拟相机(回放/合成)");
    info.serialNumber = QStringLiteral("VIRTUAL-0001");
    info.isConnected = false;
    return {info};
}

bool VirtualCamera::openCamera(const QString& cameraId)
{
    QMutexLocker locker(&m_mutex);
    if (m_isOpen) return true;

    m_currentCamera.id = QStringLiteral("virtual");
    m_currentCamera.vendor = QStringLiteral("LW Vision");
    m_currentCamera.model = QStringLiteral("虚拟相机(回放/合成)");
    m_currentCamera.serialNumber = QStringLiteral("VIRTUAL-0001");
    m_currentCamera.isConnected = true;

    // 重新扫描回放目录
    m_folderImages.clear();
    m_folderIndex = 0;
    QDir dir(m_imageDir);
    if (dir.exists()) {
        for (const QFileInfo& fi : dir.entryInfoList(
                 QStringList{"*.png", "*.jpg", "*.jpeg", "*.bmp"},
                 QDir::Files, QDir::Name)) {
            m_folderImages << fi.absoluteFilePath();
        }
    }

    m_isOpen = true;
    locker.unlock();

    if (!m_folderImages.isEmpty()) {
        VI_LOG_INFO(QString("虚拟相机: 回放目录 %1 共 %2 张图")
                    .arg(m_imageDir).arg(m_folderImages.size()));
    } else {
        VI_LOG_INFO("虚拟相机: 未找到回放图片, 使用内置合成图案");
    }
    return true;
}

void VirtualCamera::closeCamera()
{
    stopAcquisition();
    QMutexLocker locker(&m_mutex);
    m_isOpen = false;
    m_currentCamera.isConnected = false;
}

CameraParams VirtualCamera::getParams() const
{
    QMutexLocker locker(&m_mutex);
    return m_params;
}

bool VirtualCamera::setParams(const CameraParams& params)
{
    bool needRestart = m_isAcquiring;
    if (needRestart) stopAcquisition();
    {
        QMutexLocker locker(&m_mutex);
        m_params = params;
    }
    if (needRestart) startAcquisition();
    return true;
}

bool VirtualCamera::startAcquisition()
{
    QMutexLocker locker(&m_mutex);
    if (!m_isOpen) return false;
    if (m_isAcquiring) return true;
    m_isAcquiring = true;

    int interval = 33;
    if (m_params.frameRate > 0.1)
        interval = qBound(1, int(1000.0 / m_params.frameRate), 10000);
    locker.unlock();

    QMetaObject::invokeMethod(m_playTimer, "start", Qt::QueuedConnection,
                              Q_ARG(int, interval));
    VI_LOG_INFO(QString("虚拟相机: 开始采集, 帧率 %1 fps").arg(1000.0 / interval, 0, 'f', 1));
    return true;
}

bool VirtualCamera::stopAcquisition()
{
    QMetaObject::invokeMethod(m_playTimer, "stop", Qt::QueuedConnection);
    QMutexLocker locker(&m_mutex);
    if (!m_isAcquiring) return true;
    m_isAcquiring = false;
    return true;
}

CvImage VirtualCamera::grabFrame(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    QMutexLocker locker(&m_mutex);
    // 等待比"上次消费"更新的一帧
    while (m_frameGen <= m_consumedGen) {
        if (timer.elapsed() >= timeoutMs) {
            // 超时: 若缓存里一帧都没有则返回空; 有则返回当前帧(不推进消费代数)
            return m_latestFrame.clone();
        }
        m_frameCond.wait(&m_mutex, qMax(1, timeoutMs - int(timer.elapsed())));
    }
    m_consumedGen = m_frameGen;
    return m_latestFrame.clone();
}

bool VirtualCamera::triggerOnce()
{
    if (!m_isOpen) return false;
    CvImage frame = produceNextFrame();
    if (frame.empty()) return false;
    deliverFrame(frame);
    return true;
}

void VirtualCamera::setImageSourceDir(const QString& dir)
{
    QMutexLocker locker(&m_mutex);
    m_imageDir = dir;
    m_folderImages.clear();
    m_folderIndex = 0;
    QDir d(dir);
    if (d.exists()) {
        for (const QFileInfo& fi : d.entryInfoList(
                 QStringList{"*.png", "*.jpg", "*.jpeg", "*.bmp"},
                 QDir::Files, QDir::Name)) {
            m_folderImages << fi.absoluteFilePath();
        }
    }
}

void VirtualCamera::onPlaybackTick()
{
    CvImage frame = produceNextFrame();
    if (!frame.empty())
        deliverFrame(frame);
}

CvImage VirtualCamera::produceNextFrame()
{
    QMutexLocker locker(&m_mutex);
    ++m_frameIndex;

#ifdef VI_HAS_OPENCV
    // 1) 文件夹回放
    if (!m_folderImages.isEmpty()) {
        QString path = m_folderImages[m_folderIndex % m_folderImages.size()];
        ++m_folderIndex;
        cv::Mat img = cv::imread(path.toLocal8Bit().toStdString(), cv::IMREAD_COLOR);
        if (!img.empty()) {
            // 若参数要求了不同分辨率, 缩放
            if (m_params.width > 0 && m_params.height > 0 &&
                (img.cols != m_params.width || img.rows != m_params.height)) {
                cv::resize(img, img, cv::Size(m_params.width, m_params.height));
            }
            return img;
        }
    }

    // 2) 内置合成图案
    int w = m_params.width > 0 ? m_params.width : 1280;
    int h = m_params.height > 0 ? m_params.height : 1024;
    return makeSyntheticFrame(w, h, m_frameIndex);
#else
    (void)w; (void)h;
    return CvImage();
#endif
}

void VirtualCamera::deliverFrame(const CvImage& frame)
{
    {
        QMutexLocker locker(&m_mutex);
        m_latestFrame = frame.clone();
        m_frameGen++;
        m_frameCond.wakeAll();
    }
    emit imageReceived(frame);
}

#ifdef VI_HAS_OPENCV
/**
 * 合成一帧"转盘检测"图案:
 *   深灰背景 + 亮环(玻璃盘) + 一个带"外螺纹"的圆形工件 + 周期性缺陷
 * 缺陷设计 (frameIndex % 5):
 *   0-3: 正常牙型 (相位随帧号旋转, 模拟转盘转动)
 *   4:   NG件 — 牙型出现一个明显的缺口(缺牙) + 亮点(毛刺)
 */
CvImage VirtualCamera::makeSyntheticFrame(int width, int height, int frameIndex)
{
    cv::Mat img(height, width, CV_8UC1, cv::Scalar(48));

    // 玻璃盘边缘亮环
    cv::circle(img, cv::Point(width / 2, height / 2), int(std::min(width, height) * 0.48),
               cv::Scalar(70), 3, cv::LINE_AA);

    // 工件中心随帧号小幅移动 (模拟转盘上的零件经过视野)
    double angle = frameIndex * 0.7 * CV_PI / 180.0;
    cv::Point2f center(width * 0.5f + float(std::cos(angle) * width * 0.12),
                       height * 0.5f + float(std::sin(angle) * height * 0.12));
    double partRadius = std::min(width, height) * 0.18;

    // 工件本体
    cv::circle(img, center, int(partRadius), cv::Scalar(210), -1, cv::LINE_AA);

    // 外螺纹: 用径向短线段模拟牙型, 相位随帧号旋转
    bool ng = (frameIndex % 5 == 0);
    int teeth = 36;
    double phase = frameIndex * 11.0 * CV_PI / 180.0;
    for (int i = 0; i < teeth; ++i) {
        double a0 = phase + i * 2.0 * CV_PI / teeth;
        // 缺牙: NG帧在 90°~130° 区间不画牙
        double deg = a0 * 180.0 / CV_PI;
        deg = std::fmod(std::fmod(deg, 360.0) + 360.0, 360.0);
        if (ng && deg > 70.0 && deg < 130.0) continue;

        double r1 = partRadius * 0.80, r2 = partRadius * 0.97;
        cv::Point2f p1(center.x + float(std::cos(a0) * r1),
                       center.y + float(std::sin(a0) * r1));
        cv::Point2f p2(center.x + float(std::cos(a0) * r2),
                       center.y + float(std::sin(a0) * r2));
        cv::line(img, p1, p2, cv::Scalar(40), 3, cv::LINE_AA);
    }

    // 中心孔
    cv::circle(img, center, int(partRadius * 0.22), cv::Scalar(120), -1, cv::LINE_AA);

    // NG件: 缺口区加"毛刺"亮点
    if (ng) {
        cv::Point2f spike(center.x + float(std::cos(phase + 1.7) * partRadius * 0.95),
                          center.y + float(std::sin(phase + 1.7) * partRadius * 0.95));
        cv::circle(img, spike, 6, cv::Scalar(255), -1, cv::LINE_AA);
    }

    // 帧号水印
    cv::putText(img, std::string("frame ") + std::to_string(frameIndex) + (ng ? "  NG" : "  OK"),
                cv::Point(12, 26), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(200), 1, cv::LINE_AA);

    return img;
}
#endif

} // namespace VisionInspector
