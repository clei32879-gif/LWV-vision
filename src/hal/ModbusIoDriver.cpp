/**
 * @file ModbusIoDriver.cpp
 * @brief Modbus IO 驱动实现
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

bool ModbusIoDriver::parseParams(const QString& params, QString* err) {
    const QStringList parts = params.split(';', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const int eq = part.indexOf('=');
        if (eq < 0) continue;
        const QString key = part.left(eq).trimmed();
        const QString val = part.mid(eq + 1).trimmed();

        if (key == "transport")            m_useTcp = (val.compare("tcp", Qt::CaseInsensitive) == 0);
        else if (key == "host")            m_host = val;
        else if (key == "port")            m_port = quint16(val.toUShort());
        else if (key == "serialPort")      m_serialPort = val;
        else if (key == "baudRate" || key == "baud") m_baudRate = val.toInt();
        else if (key == "slaveId")         m_slaveId = val.toInt();
        else if (key == "useCoil")         m_useCoil = (val.toInt() != 0);
        else if (key == "outBase")         m_outBase = val.toInt();
        else if (key == "inBase")          m_inBase = val.toInt();
        else if (key == "inputCount")      m_inputCount = val.toInt();
        else if (key == "outputCount")     m_outputCount = val.toInt();
    }
    if (m_inputCount < 0 || m_inputCount > 64) { if (err) *err = "inputCount越界(0-64)"; return false; }
    if (m_outputCount < 0 || m_outputCount > 64) { if (err) *err = "outputCount越界(0-64)"; return false; }
    return true;
}

bool ModbusIoDriver::connect(const QString& params) {
    disconnect();
    QString err;
    if (!parseParams(params, &err)) {
        emit errorOccurred(QStringLiteral("IO参数错误: ") + err);
        return false;
    }

    if (m_useTcp) {
        m_tcp = ModbusTcpMaster::acquire(m_host, m_port, &err);
        if (!m_tcp) {
            emit errorOccurred(QStringLiteral("Modbus TCP 连接失败: ") + err);
            return false;
        }
        // 探测一次确保可达 (与 ModbusPLCDriver 一致), 避免"假已连接"
        QString probeErr;
        bool probeOk = false;
        if (m_useCoil) {
            const QVector<bool> bits = m_tcp->readBits(1, m_outBase, 1, m_slaveId, &probeErr);
            probeOk = (bits.size() == 1);
        } else {
            const QVector<quint16> regs = m_tcp->readRegisters(3, m_outBase, 1, m_slaveId, &probeErr);
            probeOk = (regs.size() == 1);
        }
        if (!probeOk) {
            m_tcp = nullptr;
            emit errorOccurred(QStringLiteral("IO目标无响应: ") + probeErr);
            return false;
        }
    } else {
        ModbusRtuMaster::SerialParams sp;
        sp.portName = m_serialPort;
        sp.baudRate = m_baudRate;
        sp.timeoutMs = 500;
        m_rtu = ModbusRtuMaster::acquire(sp, &err);
        if (!m_rtu || !m_rtu->ensureConnected(&err)) {
            m_rtu = nullptr;
            emit errorOccurred(QStringLiteral("Modbus RTU 连接失败: ") + err);
            return false;
        }
    }
    m_connected = true;
    VI_LOG_INFO(QStringLiteral("ModbusIO 已连接: %1 (slaveId=%2, outBase=%3)")
                .arg(m_useTcp ? (m_host + ":" + QString::number(m_port)) : m_serialPort)
                .arg(m_slaveId).arg(m_outBase));
    return true;
}

void ModbusIoDriver::disconnect() {
    m_tcp = nullptr;
    m_rtu = nullptr;
    m_connected = false;
}

bool ModbusIoDriver::isConnected() const {
    return m_connected;
}

bool ModbusIoDriver::readInput(int channel, bool& value) {
    if (!m_connected || channel < 0 || channel >= m_inputCount) return false;
    QString err;
    const int addr = m_inBase + channel;
    QVector<bool> bits;
    if (m_tcp)      bits = m_tcp->readBits(1, addr, 1, m_slaveId, &err);
    else if (m_rtu) bits = m_rtu->readBits(1, addr, 1, m_slaveId, &err);
    if (bits.size() != 1) {
        emit errorOccurred(QStringLiteral("读输入失败: ") + err);
        return false;
    }
    value = bits[0];
    return true;
}

bool ModbusIoDriver::readAllInputs(QVector<bool>& values) {
    if (!m_connected) return false;
    QString err;
    const int addr = m_inBase;
    QVector<bool> bits;
    if (m_tcp)      bits = m_tcp->readBits(1, addr, m_inputCount, m_slaveId, &err);
    else if (m_rtu) bits = m_rtu->readBits(1, addr, m_inputCount, m_slaveId, &err);
    if (bits.size() != m_inputCount) {
        emit errorOccurred(QStringLiteral("读全部输入失败: ") + err);
        return false;
    }
    values = bits;
    return true;
}

bool ModbusIoDriver::writeOutput(int channel, bool value) {
    if (!m_connected || channel < 0 || channel >= m_outputCount) return false;
    QString err;
    const int addr = m_outBase + channel;

    bool ok = false;
    if (m_useCoil) {
        if (m_tcp)      ok = m_tcp->writeCoil(addr, value, m_slaveId, &err);
        else if (m_rtu) ok = m_rtu->writeCoil(addr, value, m_slaveId, &err);
    } else {
        const QVector<quint16> one = {quint16(value ? 1 : 0)};
        if (m_tcp)      ok = m_tcp->writeRegisters(addr, one, m_slaveId, &err);
        else if (m_rtu) ok = m_rtu->writeRegisters(addr, one, m_slaveId, &err);
    }
    if (!ok) {
        emit errorOccurred(QStringLiteral("写输出失败: ") + err);
        return false;
    }
    return true;
}

bool ModbusIoDriver::writeAllOutputs(const QVector<bool>& values) {
    if (!m_connected || values.size() != m_outputCount) return false;
    for (int i = 0; i < values.size(); ++i) {
        if (!writeOutput(i, values[i])) return false;
    }
    return true;
}

bool ModbusIoDriver::pulseOutput(int channel, int durationMs) {
    // 脉冲: 置ON → 延时 → 复位。同步阻塞(调用方在工作线程/专用IO线程)。
    if (!writeOutput(channel, true)) return false;
    if (durationMs > 0) QThread::msleep(durationMs);
    return writeOutput(channel, false);
}

} // namespace VisionInspector
