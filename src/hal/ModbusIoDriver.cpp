/**
 * @file ModbusIoDriver.cpp
 * @brief Modbus IO 驱动实现 — 经 PLC Modbus 通道替代独立IO卡
 */

#include "ModbusIoDriver.h"
#include "../utils/Logger.h"
#include <QThread>

namespace VisionInspector {

ModbusIoDriver::ModbusIoDriver(QObject* parent)
    : IIODriver(parent)
{
}

ModbusIoDriver::~ModbusIoDriver() {
    disconnect();
}

void ModbusIoDriver::releaseMaster() {
    // 连接池由 ModbusTcpMaster/ModbusRtuMaster 静态管理, 这里只置空指针
    m_tcp = nullptr;
    m_rtu = nullptr;
}

bool ModbusIoDriver::parseParams(const QString& params, QString* err) {
    // 形如 "transport=tcp;host=...;port=502;slaveId=1;outBase=0;coilOut=0;..."
    const QStringList parts = params.split(';', Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        const int eq = p.indexOf('=');
        if (eq < 0) continue;
        const QString key = p.left(eq).trimmed();
        const QString val = p.mid(eq + 1).trimmed();
        if (key == "transport") m_p.useTcp = (val.compare("tcp", Qt::CaseInsensitive) == 0);
        else if (key == "host") m_p.host = val;
        else if (key == "port") m_p.port = quint16(val.toUShort());
        else if (key == "serialPort" || key == "portname") m_p.serialPort = val;
        else if (key == "baud" || key == "baudrate") m_p.baudRate = val.toInt();
        else if (key == "slaveId" || key == "slaveid") m_p.slaveId = val.toInt();
        else if (key == "coilOut" || key == "coilout") m_p.coilOut = (val.toInt() != 0);
        else if (key == "outBase" || key == "outbase") m_p.outBase = val.toInt();
        else if (key == "inBase" || key == "inbase") m_p.inBase = val.toInt();
        else if (key == "readInputAsCoil") m_p.readInputAsCoil = (val.toInt() != 0);
        else if (key == "timeoutMs") m_p.timeoutMs = val.toInt();
        else if (key == "inputCount") m_p.inputCount = val.toInt();
        else if (key == "outputCount") m_p.outputCount = val.toInt();
        else if (key == "parity") { /* 忽略, 默认无校验 */ }
    }
    if (m_p.inputCount < 0 || m_p.inputCount > 64) { if (err) *err = "inputCount越界"; return false; }
    if (m_p.outputCount < 0 || m_p.outputCount > 64) { if (err) *err = "outputCount越界"; return false; }
    return true;
}

bool ModbusIoDriver::connect(const QString& params) {
    QMutexLocker locker(&m_mutex);
    disconnectInternal();
    QString err;
    if (!parseParams(params, &err)) {
        emit errorOccurred("IO参数错误: " + err);
        return false;
    }

    if (m_p.useTcp) {
        m_tcp = ModbusTcpMaster::acquire(m_p.host, m_p.port, &err);
        if (!m_tcp) {
            emit errorOccurred("Modbus TCP 连接失败: " + err);
            return false;
        }
        // 无独立TCP连接状态, 以后续读写为准
        m_connected = true;
    } else {
        ModbusRtuMaster::SerialParams sp;
        sp.portName = m_p.serialPort;
        sp.baudRate = m_p.baudRate;
        sp.timeoutMs = m_p.timeoutMs;
        m_rtu = ModbusRtuMaster::acquire(sp, &err);
        if (!m_rtu || !m_rtu->ensureConnected(&err)) {
            m_rtu = nullptr;
            emit errorOccurred("Modbus RTU 连接失败: " + err);
            return false;
        }
        m_connected = true;
    }
    VI_LOG_INFO("ModbusIO 已连接: " + driverName());
    return true;
}

void ModbusIoDriver::disconnect() {
    QMutexLocker locker(&m_mutex);
    disconnectInternal();
}

void ModbusIoDriver::disconnectInternal() {
    releaseMaster();
    m_connected = false;
}

bool ModbusIoDriver::isConnected() const {
    return m_connected;
}

bool ModbusIoDriver::readInput(int channel, bool& value) {
    if (!m_connected || channel < 0 || channel >= m_p.inputCount) return false;
    QString err;
    QVector<bool> bits;
    if (m_p.readInputAsCoil) {
        if (m_tcp) bits = m_tcp->readBits(1, m_p.inBase + channel, 1, m_p.slaveId, &err);
        else if (m_rtu) bits = m_rtu->readBits(1, m_p.inBase + channel, 1, m_p.slaveId, &err);
    } else {
        if (m_tcp) bits = m_tcp->readBits(2, m_p.inBase + channel, 1, m_p.slaveId, &err);
        else if (m_rtu) bits = m_rtu->readBits(2, m_p.inBase + channel, 1, m_p.slaveId, &err);
    }
    if (bits.size() != 1) {
        emit errorOccurred("IO读输入失败: " + err);
        return false;
    }
    value = bits[0];
    return true;
}

bool ModbusIoDriver::readAllInputs(QVector<bool>& values) {
    if (!m_connected) return false;
    QString err;
    QVector<bool> bits;
    if (m_p.readInputAsCoil) {
        if (m_tcp) bits = m_tcp->readBits(1, m_p.inBase, m_p.inputCount, m_p.slaveId, &err);
        else if (m_rtu) bits = m_rtu->readBits(1, m_p.inBase, m_p.inputCount, m_p.slaveId, &err);
    } else {
        if (m_tcp) bits = m_tcp->readBits(2, m_p.inBase, m_p.inputCount, m_p.slaveId, &err);
        else if (m_rtu) bits = m_rtu->readBits(2, m_p.inBase, m_p.inputCount, m_p.slaveId, &err);
    }
    if (bits.size() != m_p.inputCount) {
        emit errorOccurred("IO读全部输入失败: " + err);
        return false;
    }
    values = bits;
    return true;
}

bool ModbusIoDriver::writeOutput(int channel, bool value) {
    if (!m_connected || channel < 0 || channel >= m_p.outputCount) return false;
    QString err;
    const int addr = m_p.outBase + channel;

    if (m_p.coilOut) {
        bool ok = false;
        if (m_tcp) ok = m_tcp->writeCoil(addr, value, m_p.slaveId, &err);
        else if (m_rtu) ok = m_rtu->writeCoil(addr, value, m_p.slaveId, &err);
        if (!ok) { emit errorOccurred("IO写输出失败: " + err); return false; }
        return true;
    }

    // 寄存器模式: 筛选机结果数组 K1=OK K2=NG, 关=0
    const quint16 v = value ? (channel < 8 ? 1 : 2) : 0;  // 相机通道写K1, 剔除通道写K2(按契约微调)
    QVector<quint16> one = {v};
    bool ok = false;
    if (m_tcp) ok = m_tcp->writeRegisters(addr, one, m_p.slaveId, &err);
    else if (m_rtu) ok = m_rtu->writeRegisters(addr, one, m_p.slaveId, &err);
    if (!ok) { emit errorOccurred("IO写输出失败: " + err); return false; }
    return true;
}

bool ModbusIoDriver::writeAllOutputs(const QVector<bool>& values) {
    if (!m_connected || values.size() != m_p.outputCount) return false;
    for (int i = 0; i < values.size(); ++i)
        if (!writeOutput(i, values[i])) return false;
    return true;
}

bool ModbusIoDriver::pulseOutput(int channel, int durationMs) {
    // 同步脉冲: 置ON → 延时 → 复位。仅在流程工作线程调用(execute在工作线程),
    // 不依赖事件循环/定时器, 现场时序确定性强。
    if (!writeOutput(channel, true)) return false;
    if (durationMs > 0) QThread::msleep(durationMs);
    return writeOutput(channel, false);
}

} // namespace VisionInspector
