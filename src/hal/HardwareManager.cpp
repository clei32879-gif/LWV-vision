/**
 * @file HardwareManager.cpp
 * @brief 硬件管理器实现
 */

#include "HardwareManager.h"
#include "ModbusIoDriver.h"
#include "ModbusPLCDriver.h"
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
    // PLC 驱动经自研 Modbus 主站实现 (信捷 XD 系列, 见契约与路线图阶段5)
    disconnectPLC();
    auto* plc = new ModbusPLCDriver(this);
    if (!plc->connect(params)) {
        VI_LOG_ERROR(QStringLiteral("PLC 连接失败: ") + params.ipAddress);
        delete plc;
        return false;
    }
    m_plc = plc;
    emit plcConnected();
    return true;
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
    // IO 驱动经 Modbus 通道实现 (替代独立IO卡如 N1616):
    //   - 有 PLC 时直写 PLC M 线圈/结果寄存器 (免硬件 IO 卡)
    //   - 或配远程IO模块 (ICP DAS ET-7042/I-8057 等, 见选型报告)
    // 参数示例:
    //   transport=tcp;host=192.168.1.10;slaveId=1;outBase=106;useCoil=1;inBase=7;inputCount=16;outputCount=16
    disconnectIO();
    auto* io = new ModbusIoDriver(this);
    if (!io->connect(params)) {
        VI_LOG_ERROR(QStringLiteral("IO驱动连接失败: ") + params);
        delete io;
        return false;
    }
    m_io = io;
    VI_LOG_INFO(QStringLiteral("IO驱动已连接: ") + io->driverName());
    return true;
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
