/**
 * @file HardwareManager.h
 * @brief 硬件设备管理器
 *
 * 统一管理所有硬件设备(相机、PLC、伺服、IO)的连接和生命周期。
 * 软件启动时创建, 退出时销毁。
 */

#pragma once

#include "ICameraDriver.h"
#include "IPLCDriver.h"
#include "IServoDriver.h"
#include "IIODriver.h"
#include <QObject>
#include <QList>
#include <QString>
#include <memory>

namespace VisionInspector {

class HardwareManager : public QObject {
    Q_OBJECT

public:
    explicit HardwareManager(QObject* parent = nullptr);
    ~HardwareManager();

    // 相机管理 (支持1-8台)
    QList<CameraInfo> enumerateAllCameras();
    bool openCamera(int index, const QString& cameraId);
    void closeCamera(int index);
    ICameraDriver* cameraAt(int index);
    int cameraCount() const { return m_cameras.size(); }

    // PLC管理
    bool connectPLC(const PLCConnectionParams& params);
    void disconnectPLC();
    IPLCDriver* plc() { return m_plc; }

    // 伺服管理
    bool connectServo(const QString& port);
    void disconnectServo();
    IServoDriver* servo() { return m_servo; }

    // IO管理
    bool connectIO(const QString& params);
    void disconnectIO();
    IIODriver* io() { return m_io; }

    // 注册相机驱动工厂 (插件调用)
    using CameraDriverFactory = std::function<ICameraDriver*()>;
    void registerCameraDriver(const QString& name, CameraDriverFactory factory);

    // 注册PLC驱动工厂
    using PLCDriverFactory = std::function<IPLCDriver*()>;
    void registerPLCDriver(const QString& name, PLCDriverFactory factory);

signals:
    void cameraConnected(int index, const CameraInfo& info);
    void cameraDisconnected(int index);
    void plcConnected();
    void plcDisconnected();
    void errorOccurred(const QString& error);

private:
    QList<ICameraDriver*> m_cameras;      // 最多8台
    IPLCDriver* m_plc = nullptr;
    IServoDriver* m_servo = nullptr;
    IIODriver* m_io = nullptr;

    // 驱动工厂
    QMap<QString, CameraDriverFactory> m_cameraFactories;
    QMap<QString, PLCDriverFactory> m_plcFactories;
};

} // namespace VisionInspector
