/**
 * @file ModbusPLCDriver.cpp
 * @brief 信捷 XD Modbus PLC 驱动实现
 */

#include "ModbusPLCDriver.h"
#include "../utils/Logger.h"
#include <QThread>

namespace VisionInspector {

// 契约地址常量 (保持寄存器 = 41088 + D号)
namespace {
constexpr int kRegBaseD = 41088;
constexpr int kRegManualSpeed = kRegBaseD + 0;     // HD0  手动速度
constexpr int kRegCmd = kRegBaseD + 6;             // D6   上位机命令
constexpr int kRegAutoSpeed = kRegBaseD + 8;       // HD8  自动速度
constexpr int kRegYield = kRegBaseD + 122;         // HD122 良率
constexpr int kRegOKCount = kRegBaseD + 170;       // HD170 OK产量
constexpr int kRegNGCount = kRegBaseD + 172;       // HD172 NG产量
constexpr int kRegTotalCount = kRegBaseD + 174;    // HD174 总产量
constexpr int kRegUph = kRegBaseD + 196;           // D196  UPH
constexpr int kRegServoPos = kRegBaseD + 220;      // HD220 伺服位置(2字)

constexpr int kCoilServoFwd = 50;                  // M50
constexpr int kCoilServoRev = 52;                  // M52
constexpr int kCoilOKValve = 106;                  // M106 OK阀
constexpr int kCoilNG1Valve = 107;                 // M107 NG1阀
constexpr int kCoilNG2Valve = 108;                 // M108 NG2阀
constexpr int kCoilManualOK = 116;                 // M116 手动OK
constexpr int kCoilManualNG = 117;                 // M117 手动NG
constexpr int kCoilManualShotBase = 121;           // M121..M135 手动拍照
}

ModbusPLCDriver::ModbusPLCDriver(QObject* parent)
    : IPLCDriver(parent)
{
}

ModbusPLCDriver::~ModbusPLCDriver() {
    disconnect();
}

void ModbusPLCDriver::releaseMaster() {
    // 连接池由 ModbusTcpMaster/ModbusRtuMaster 静态管理, 这里只置空指针
    m_tcp = nullptr;
    m_rtu = nullptr;
}

bool ModbusPLCDriver::connect(const PLCConnectionParams& params) {
    disconnect();
    m_useTcp = (params.commType == PLCCommType::ModbusTCP);
    m_slaveId = params.slaveId > 0 ? params.slaveId : 1;

    QString err;
    if (m_useTcp) {
        m_tcp = ModbusTcpMaster::acquire(params.ipAddress, quint16(params.port > 0 ? params.port : 502), &err);
        if (!m_tcp) {
            emit errorOccurred(QStringLiteral("Modbus TCP 连接失败: ") + err);
            return false;
        }
        // 主动探测一次, 确保 PLC 可达
        QVector<quint16> probe = m_tcp->readRegisters(3, kRegManualSpeed, 1, m_slaveId, &err);
        if (probe.isEmpty()) {
            emit errorOccurred(QStringLiteral("PLC 无响应: ") + err);
            m_tcp = nullptr;
            return false;
        }
    } else {
        ModbusRtuMaster::SerialParams sp;
        sp.portName = params.serialPort;
        sp.baudRate = params.baudRate > 0 ? params.baudRate : 9600;
        sp.timeoutMs = params.timeoutMs > 0 ? params.timeoutMs : 1000;
        m_rtu = ModbusRtuMaster::acquire(sp, &err);
        if (!m_rtu || !m_rtu->ensureConnected(&err)) {
            m_rtu = nullptr;
            emit errorOccurred(QStringLiteral("Modbus RTU 连接失败: ") + err);
            return false;
        }
    }
    m_connected = true;
    emit connectionChanged(true);
    VI_LOG_INFO(QStringLiteral("PLC 已连接: %1 (slaveId=%2)")
                .arg(m_useTcp ? params.ipAddress + ":" + QString::number(params.port) : params.serialPort)
                .arg(m_slaveId));
    return true;
}

void ModbusPLCDriver::disconnect() {
    if (m_connected) {
        releaseMaster();
        m_connected = false;
        emit connectionChanged(false);
    }
}

bool ModbusPLCDriver::isConnected() const {
    return m_connected;
}

bool ModbusPLCDriver::readHoldingRegisters(int address, int count, QVector<quint16>& values) {
    if (!m_connected) return false;
    QString e;
    if (m_tcp)      values = m_tcp->readRegisters(3, address, count, m_slaveId, &e);
    else if (m_rtu) values = m_rtu->readRegisters(3, address, count, m_slaveId, &e);
    if (values.size() != count) { emit errorOccurred(QStringLiteral("读保持寄存器失败: ") + e); return false; }
    return true;
}

bool ModbusPLCDriver::writeHoldingRegisters(int address, const QVector<quint16>& values) {
    if (!m_connected || values.isEmpty()) return false;
    QString e;
    bool ok = false;
    if (m_tcp)      ok = m_tcp->writeRegisters(address, values, m_slaveId, &e);
    else if (m_rtu) ok = m_rtu->writeRegisters(address, values, m_slaveId, &e);
    if (!ok) { emit errorOccurred(QStringLiteral("写保持寄存器失败: ") + e); return false; }
    return true;
}

bool ModbusPLCDriver::readCoils(int address, int count, QVector<bool>& values) {
    if (!m_connected) return false;
    QString e;
    if (m_tcp)      values = m_tcp->readBits(1, address, count, m_slaveId, &e);
    else if (m_rtu) values = m_rtu->readBits(1, address, count, m_slaveId, &e);
    if (values.size() != count) { emit errorOccurred(QStringLiteral("读线圈失败: ") + e); return false; }
    return true;
}

bool ModbusPLCDriver::writeCoils(int address, const QVector<bool>& values) {
    if (!m_connected || values.isEmpty()) return false;
    QString e;
    bool ok = false;
    if (m_tcp)      ok = m_tcp->writeCoils(address, values, m_slaveId, &e);
    else if (m_rtu) ok = m_rtu->writeCoils(address, values, m_slaveId, &e);
    if (!ok) { emit errorOccurred(QStringLiteral("写线圈失败: ") + e); return false; }
    return true;
}

bool ModbusPLCDriver::readInputRegisters(int address, int count, QVector<quint16>& values) {
    if (!m_connected) return false;
    QString e;
    if (m_tcp)      values = m_tcp->readRegisters(4, address, count, m_slaveId, &e);
    else if (m_rtu) values = m_rtu->readRegisters(4, address, count, m_slaveId, &e);
    if (values.size() != count) { emit errorOccurred(QStringLiteral("读输入寄存器失败: ") + e); return false; }
    return true;
}

bool ModbusPLCDriver::readDiscreteInputs(int address, int count, QVector<bool>& values) {
    if (!m_connected) return false;
    QString e;
    if (m_tcp)      values = m_tcp->readBits(2, address, count, m_slaveId, &e);
    else if (m_rtu) values = m_rtu->readBits(2, address, count, m_slaveId, &e);
    if (values.size() != count) { emit errorOccurred(QStringLiteral("读离散输入失败: ") + e); return false; }
    return true;
}

// ---------------- 业务级 ----------------

bool ModbusPLCDriver::blowOK(int durationMs) {
    if (!m_connected) return false;
    QString e;
    bool ok = false;
    if (m_tcp)      ok = m_tcp->writeCoil(kCoilOKValve, true, m_slaveId, &e);
    else if (m_rtu) ok = m_rtu->writeCoil(kCoilOKValve, true, m_slaveId, &e);
    if (!ok) { emit errorOccurred(QStringLiteral("OK吹气置位失败: ") + e); return false; }
    if (durationMs > 0) QThread::msleep(durationMs);
    if (m_tcp)      ok = m_tcp->writeCoil(kCoilOKValve, false, m_slaveId, &e);
    else if (m_rtu) ok = m_rtu->writeCoil(kCoilOKValve, false, m_slaveId, &e);
    return ok;
}

bool ModbusPLCDriver::blowNG(int durationMs) {
    if (!m_connected) return false;
    QString e;
    bool ok = false;
    if (m_tcp)      ok = m_tcp->writeCoil(kCoilNG1Valve, true, m_slaveId, &e);
    else if (m_rtu) ok = m_rtu->writeCoil(kCoilNG1Valve, true, m_slaveId, &e);
    if (!ok) { emit errorOccurred(QStringLiteral("NG吹气置位失败: ") + e); return false; }
    if (durationMs > 0) QThread::msleep(durationMs);
    if (m_tcp)      ok = m_tcp->writeCoil(kCoilNG1Valve, false, m_slaveId, &e);
    else if (m_rtu) ok = m_rtu->writeCoil(kCoilNG1Valve, false, m_slaveId, &e);
    return ok;
}

qint64 ModbusPLCDriver::readServoPosition() {
    QVector<quint16> v;
    if (!readHoldingRegisters(kRegServoPos, 2, v)) return 0;
    return qint64((quint32(v[1]) << 16) | v[0]);
}

bool ModbusPLCDriver::setDiskSpeed(int rpm) {
    return writeHoldingRegisters(kRegManualSpeed, QVector<quint16>{quint16(rpm & 0xFFFF)});
}

bool ModbusPLCDriver::isMaterialReady() {
    QVector<bool> v;
    if (!readDiscreteInputs(0, 1, v)) return false;   // X0 物料到位
    return v.value(0, false);
}

// ---------------- 契约便捷接口 ----------------

bool ModbusPLCDriver::setManualSpeed(int rpm) {
    return setDiskSpeed(rpm);
}

bool ModbusPLCDriver::setAutoSpeed(int rpm) {
    return writeHoldingRegisters(kRegAutoSpeed, QVector<quint16>{quint16(rpm & 0xFFFF)});
}

bool ModbusPLCDriver::sendCommand(int cmd) {
    return writeHoldingRegisters(kRegCmd, QVector<quint16>{quint16(cmd & 0xFFFF)});
}

int ModbusPLCDriver::readOKCount() {
    QVector<quint16> v;
    return readHoldingRegisters(kRegOKCount, 1, v) ? int(v[0]) : -1;
}

int ModbusPLCDriver::readNGCount() {
    QVector<quint16> v;
    return readHoldingRegisters(kRegNGCount, 1, v) ? int(v[0]) : -1;
}

int ModbusPLCDriver::readTotalCount() {
    QVector<quint16> v;
    return readHoldingRegisters(kRegTotalCount, 1, v) ? int(v[0]) : -1;
}

int ModbusPLCDriver::readYieldPermille() {
    QVector<quint16> v;
    return readHoldingRegisters(kRegYield, 1, v) ? int(v[0]) : -1;
}

int ModbusPLCDriver::readUph() {
    QVector<quint16> v;
    return readHoldingRegisters(kRegUph, 1, v) ? int(v[0]) : -1;
}

bool ModbusPLCDriver::manualShot(int camera) {
    if (camera < 1 || camera > 8) return false;
    return writeCoils(kCoilManualShotBase + camera - 1, QVector<bool>{true});
}

bool ModbusPLCDriver::manualOK() {
    return writeCoils(kCoilManualOK, QVector<bool>{true});
}

bool ModbusPLCDriver::manualNG() {
    return writeCoils(kCoilManualNG, QVector<bool>{true});
}

bool ModbusPLCDriver::servoForward(bool on) {
    return writeCoils(kCoilServoFwd, QVector<bool>{on});
}

bool ModbusPLCDriver::servoReverse(bool on) {
    return writeCoils(kCoilServoRev, QVector<bool>{on});
}

} // namespace VisionInspector
