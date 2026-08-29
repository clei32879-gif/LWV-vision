/**
 * @file HardwareManager.cpp
 * @brief 硬件管理器实现
 */

#include "HardwareManager.h"
#include "../utils/Logger.h"

namespace VisionInspector {

HardwareManager::HardwareManager(QObject* parent)
    : QObject(parent)
{
    // 预分配8个相机槽位 (支持1-8台即插即用)
    m_cameras.reserve(8);
}

HardwareManager::~HardwareManager() {
    // 关闭所有相机
    for (auto* camera : m_cameras) {
        if (camera) {
            camera->closeCamera();
            delete camera;
        }
    }
    m_cameras.clear();

    // 断开PLC
    if (m_plc) {
        m_plc->disconnect();
        delete m_plc;
    }
    // 断开伺服
    if (m_servo) {
        m_servo->disconnect();
        delete m_servo;
    }
    // 断开IO
    if (m_io) {
        m_io->disconnect();
        delete m_io;
    }
}

QList<CameraInfo> HardwareManager::enumerateAllCameras() {
    QList<CameraInfo> result;
    // 遍历所有注册的相机驱动, 枚举各自的相机
    for (auto it = m_cameraFactories.begin(); it != m_cameraFactories.end(); ++it) {
        auto* driver = it.value()();
        if (driver) {
            auto cameras = driver->enumerateCameras();
            for (const auto& cam : cameras) {
                result.append(cam);
            }
            delete driver; // 枚举后销毁临时驱动
        }
    }
    return result;
}

bool HardwareManager::openCamera(int index, const QString& cameraId) {
    // 确保index在0-7范围内
    if (index < 0 || index >= 8) {
        VI_LOG_ERROR(QString("相机索引超出范围(0-7): %1").arg(index));
        return false;
    }

    // 扩展列表到index
    while (m_cameras.size() <= index) {
        m_cameras.append(nullptr);
    }

    // 如果该位置已有相机, 先关闭
    if (m_cameras[index]) {
        m_cameras[index]->closeCamera();
        delete m_cameras[index];
        m_cameras[index] = nullptr;
    }

    // TODO: 根据cameraId找到对应的驱动工厂创建相机
    // 这里暂时返回false, 后续实现
    VI_LOG_WARN("openCamera 待实现: 需要驱动插件");
    return false;
}

void HardwareManager::closeCamera(int index) {
    if (index < 0 || index >= m_cameras.size()) return;
    if (m_cameras[index]) {
        m_cameras[index]->closeCamera();
        delete m_cameras[index];
        m_cameras[index] = nullptr;
        emit cameraDisconnected(index);
    }
}

ICameraDriver* HardwareManager::cameraAt(int index) {
    if (index < 0 || index >= m_cameras.size()) return nullptr;
    return m_cameras[index];
}

bool HardwareManager::connectPLC(const PLCConnectionParams& params) {
    // TODO: 创建PLC驱动并连接
    VI_LOG_WARN("connectPLC 待实现: 需要驱动插件");
    return false;
}

void HardwareManager::disconnectPLC() {
    if (m_plc) {
        m_plc->disconnect();
        emit plcDisconnected();
    }
}

bool HardwareManager::connectServo(const QString& port) {
    // TODO
    VI_LOG_WARN("connectServo 待实现");
    return false;
}

void HardwareManager::disconnectServo() {
    if (m_servo) {
        m_servo->disconnect();
    }
}

bool HardwareManager::connectIO(const QString& params) {
    // TODO
    return false;
}

void HardwareManager::disconnectIO() {
    if (m_io) {
        m_io->disconnect();
    }
}

void HardwareManager::registerCameraDriver(const QString& name, CameraDriverFactory factory) {
    m_cameraFactories[name] = factory;
    VI_LOG_INFO("已注册相机驱动: " + name);
}

void HardwareManager::registerPLCDriver(const QString& name, PLCDriverFactory factory) {
    m_plcFactories[name] = factory;
    VI_LOG_INFO("已注册PLC驱动: " + name);
}

} // namespace VisionInspector
