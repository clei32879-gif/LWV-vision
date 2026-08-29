/**
 * @file ModbusTcpMaster.cpp
 * @brief Modbus TCP 主站实现
 */

#include "ModbusTcpMaster.h"
#include "../utils/Logger.h"

namespace VisionInspector {

QMutex ModbusTcpMaster::s_poolMutex;
QMap<QString, ModbusTcpMaster*> ModbusTcpMaster::s_pool;

ModbusTcpMaster* ModbusTcpMaster::acquire(const QString& host, quint16 port, QString* err) {
    const QString key = host + ":" + QString::number(port);
    QMutexLocker locker(&s_poolMutex);
    auto it = s_pool.find(key);
    if (it != s_pool.end())
        return it.value();
    auto* master = new ModbusTcpMaster(host, port);
    s_pool.insert(key, master);
    if (err) err->clear();
    return master;
}

void ModbusTcpMaster::releaseAll() {
    QMutexLocker locker(&s_poolMutex);
    for (auto it = s_pool.begin(); it != s_pool.end(); ++it) {
        it.value()->disconnectFromDevice();
        it.value()->deleteLater();
    }
    s_pool.clear();
}

ModbusTcpMaster::ModbusTcpMaster(const QString& host, quint16 port, QObject* parent)
    : QObject(parent), m_host(host), m_port(port)
{
    m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
}

bool ModbusTcpMaster::isConnected() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

bool ModbusTcpMaster::ensureConnected(QString* err) {
    if (isConnected()) return true;
    m_socket.connectToHost(m_host, m_port);
    if (!m_socket.waitForConnected(3000)) {
        if (err) *err = QString("连接 %1:%2 失败: %3")
                            .arg(m_host).arg(m_port)
                            .arg(m_socket.errorString());
        return false;
    }
    VI_LOG_INFO("Modbus TCP 已连接: " + endpoint());
    return true;
}

void ModbusTcpMaster::disconnectFromDevice() {
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.disconnectFromHost();
}

QByteArray ModbusTcpMaster::transact(quint8 function, const QByteArray& pdu,
                                     int slaveId, QString* err) {
    QMutexLocker locker(&m_mutex);
    if (!ensureConnected(err)) return {};

    const quint16 txId = ++m_txId;
    QByteArray frame;
    // MBAP头: 事务ID(2) 协议ID(2)=0 长度(2)=PDU长度+1(单元ID) 单元ID(1)
    const quint16 len = quint16(pdu.size() + 1);
    frame.append(char(txId >> 8));       frame.append(char(txId & 0xFF));
    frame.append(char(0));               frame.append(char(0));
    frame.append(char(len >> 8));        frame.append(char(len & 0xFF));
    frame.append(char(slaveId & 0xFF));
    frame.append(pdu);

    m_socket.readAll();  // 丢弃残留数据
    if (m_socket.write(frame) == -1 || !m_socket.waitForBytesWritten(2000)) {
        if (err) *err = "发送失败: " + m_socket.errorString();
        m_socket.disconnectFromHost();
        return {};
    }

    // 读响应: 先凑够MBAP头, 再按长度取剩余
    QByteArray resp;
    int expectTotal = -1;
    const int deadline = 3000;
    qint64 start = QDateTime::currentMSecsSinceEpoch();
    while ((QDateTime::currentMSecsSinceEpoch() - start) < deadline) {
        if (m_socket.waitForReadyRead(200)) {
            resp += m_socket.readAll();
            if (expectTotal < 0 && resp.size() >= 6) {
                const quint16 rxLen = quint8(resp[4]) << 8 | quint8(resp[5]);
                expectTotal = 6 + rxLen;   // MBAP前6字节 + 长度字段指示的后续字节
            }
            if (expectTotal > 0 && resp.size() >= expectTotal)
                break;
        }
        if (m_socket.state() != QAbstractSocket::ConnectedState &&
            m_socket.bytesAvailable() == 0) {
            if (err) *err = "连接中断";
            return {};
        }
    }

    if (expectTotal < 0 || resp.size() < expectTotal) {
        if (err) *err = "响应超时";
        return {};
    }

    // 校验事务ID与单元ID
    const quint16 rxTxId = quint8(resp[0]) << 8 | quint8(resp[1]);
    if (rxTxId != txId) {
        if (err) *err = "事务ID不匹配";
        return {};
    }
    const quint8 rxSlave = quint8(resp[6]);
    if (rxSlave != slaveId) {
        if (err) *err = "从站ID不匹配";
        return {};
    }

    QByteArray pduResp = resp.mid(7, expectTotal - 7);
    // 异常响应: 功能码|0x80
    if (!pduResp.isEmpty() && (quint8(pduResp[0]) & 0x80)) {
        const quint8 code = pduResp.size() > 1 ? quint8(pduResp[1]) : 0;
        QString what;
        switch (code) {
            case 1: what = "非法功能"; break;
            case 2: what = "非法地址"; break;
            case 3: what = "非法数值"; break;
            case 4: what = "从站故障"; break;
            default: what = QString("异常码%1").arg(code);
        }
        if (err) *err = "Modbus异常: " + what;
        return {};
    }
    if (err) err->clear();
    return pduResp;
}

QVector<quint16> ModbusTcpMaster::readRegisters(int function, int addr, int count,
                                                int slaveId, QString* err) {
    if (count < 1 || count > 125) { if (err) *err = "数量须在1~125"; return {}; }
    QByteArray pdu;
    pdu.append(char(function));
    pdu.append(char((addr >> 8) & 0xFF));  pdu.append(char(addr & 0xFF));
    pdu.append(char((count >> 8) & 0xFF)); pdu.append(char(count & 0xFF));

    QByteArray resp = transact(quint8(function), pdu, slaveId, err);
    if (resp.isEmpty()) return {};

    if (resp.size() < 2 || quint8(resp[1]) != resp.size() - 2) {
        if (err) *err = "响应长度不符";
        return {};
    }
    QVector<quint16> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int o = 2 + i * 2;
        if (o + 1 >= resp.size()) { if (err) *err = "响应数据不足"; return {}; }
        out.append(quint16(quint8(resp[o]) << 8 | quint8(resp[o + 1])));
    }
    return out;
}

QVector<bool> ModbusTcpMaster::readBits(int function, int addr, int count,
                                        int slaveId, QString* err) {
    if (count < 1 || count > 2000) { if (err) *err = "数量须在1~2000"; return {}; }
    QByteArray pdu;
    pdu.append(char(function));
    pdu.append(char((addr >> 8) & 0xFF));  pdu.append(char(addr & 0xFF));
    pdu.append(char((count >> 8) & 0xFF)); pdu.append(char(count & 0xFF));

    QByteArray resp = transact(quint8(function), pdu, slaveId, err);
    if (resp.isEmpty()) return {};

    QVector<bool> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int byteIdx = 2 + i / 8;
        const int bitIdx = i % 8;
        if (byteIdx >= resp.size()) { if (err) *err = "响应数据不足"; return {}; }
        out.append((quint8(resp[byteIdx]) >> bitIdx) & 1);
    }
    return out;
}

bool ModbusTcpMaster::writeRegister(int addr, quint16 value, int slaveId, QString* err) {
    QByteArray pdu;
    pdu.append(char(0x06));
    pdu.append(char((addr >> 8) & 0xFF)); pdu.append(char(addr & 0xFF));
    pdu.append(char((value >> 8) & 0xFF)); pdu.append(char(value & 0xFF));
    return !transact(0x06, pdu, slaveId, err).isEmpty();
}

bool ModbusTcpMaster::writeRegisters(int addr, const QVector<quint16>& values,
                                     int slaveId, QString* err) {
    if (values.isEmpty() || values.size() > 123) {
        if (err) *err = "数量须在1~123"; return false;
    }
    QByteArray pdu;
    pdu.append(char(0x10));
    pdu.append(char((addr >> 8) & 0xFF)); pdu.append(char(addr & 0xFF));
    pdu.append(char((values.size() >> 8) & 0xFF)); pdu.append(char(values.size() & 0xFF));
    pdu.append(char(values.size() * 2));
    for (quint16 v : values) {
        pdu.append(char(v >> 8)); pdu.append(char(v & 0xFF));
    }
    return !transact(0x10, pdu, slaveId, err).isEmpty();
}

bool ModbusTcpMaster::writeCoil(int addr, bool on, int slaveId, QString* err) {
    QByteArray pdu;
    pdu.append(char(0x05));
    pdu.append(char((addr >> 8) & 0xFF)); pdu.append(char(addr & 0xFF));
    const quint16 val = on ? 0xFF00 : 0x0000;
    pdu.append(char((val >> 8) & 0xFF)); pdu.append(char(val & 0xFF));
    return !transact(0x05, pdu, slaveId, err).isEmpty();
}

bool ModbusTcpMaster::writeCoils(int addr, const QVector<bool>& bits,
                                 int slaveId, QString* err) {
    if (bits.isEmpty() || bits.size() > 1968) {
        if (err) *err = "数量须在1~1968"; return false;
    }
    QByteArray pdu;
    pdu.append(char(0x0F));
    pdu.append(char((addr >> 8) & 0xFF)); pdu.append(char(addr & 0xFF));
    pdu.append(char((bits.size() >> 8) & 0xFF)); pdu.append(char(bits.size() & 0xFF));
    const int byteCount = (bits.size() + 7) / 8;
    pdu.append(char(byteCount));
    QByteArray packed(byteCount, char(0));
    for (int i = 0; i < bits.size(); ++i) {
        if (bits[i]) packed[i / 8] = char(quint8(packed[i / 8]) | (1 << (i % 8)));
    }
    pdu.append(packed);
    return !transact(0x0F, pdu, slaveId, err).isEmpty();
}

} // namespace VisionInspector
