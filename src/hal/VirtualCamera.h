/**
 * @file VirtualCamera.h
 * @brief 虚拟相机驱动 — 无真实硬件时的开发/演示/联调相机
 *
 * 图像来源(按优先级):
 *   1. 图像文件夹回放: 按帧率循环播放指定目录下的图片
 *      (默认 appDir/testdata/virtual_camera, 可用 setImageSourceDir 更换)
 *   2. 内置合成图案: 目录无图时自动生成旋转工件图案(带周期性缺陷)
 *
 * 用途:
 *   - 无相机电脑上开发调试全流程
 *   - 算法回归测试 (固定图像序列可复现)
 *   - 展会/客户演示
 */

#pragma once

#include "ICameraDriver.h"
#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include <QStringList>

namespace VisionInspector {

class VirtualCamera : public ICameraDriver {
    Q_OBJECT

public:
    explicit VirtualCamera(QObject* parent = nullptr);
    ~VirtualCamera() override;

    // ---- ICameraDriver 接口 ----
    QString driverName() const override { return QStringLiteral("VirtualCamera"); }
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
    QStringList supportedPixelFormats() const override {
        return {QStringLiteral("Mono8"), QStringLiteral("RGB8")};
    }

    // ---- 虚拟相机专属 ----

    /** 设置回放图像目录 (空字符串 = 使用内置合成图案) */
    void setImageSourceDir(const QString& dir);
    QString imageSourceDir() const { return m_imageDir; }

    /** 生成一帧合成图像 (测试/示例用途) */
    static CvImage makeSyntheticFrame(int width, int height, int frameIndex);

private slots:
    void onPlaybackTick();

private:
    CvImage produceNextFrame();          // 产生下一帧 (调用方需持有m_mutex? 否—内部自锁)
    void deliverFrame(const CvImage& frame);

    bool m_isOpen = false;
    bool m_isAcquiring = false;
    CameraInfo m_currentCamera;
    CameraParams m_params;

    QString m_imageDir;                  // 回放目录
    QStringList m_folderImages;          // 目录内的图像文件列表
    int m_folderIndex = 0;

    int m_frameIndex = 0;                // 合成图案的帧计数

    QTimer* m_playTimer = nullptr;

    // 帧缓存与同步 (grabFrame 可能从工作线程调用)
    mutable QMutex m_mutex;
    QWaitCondition m_frameCond;
    CvImage m_latestFrame;
    quint64 m_frameGen = 0;              // 已产生帧的代数
    quint64 m_consumedGen = 0;           // grabFrame消费到的代数
};

} // namespace VisionInspector
