/**
 * @file ModbusTcpSlave.cpp
 * @brief Modbus TCP 从站模拟器实现
 */

#include "ModbusTcpSlave.h"
#include "../utils/Logger.h"

#include <cmath>

namespace VisionInspector {

namespace {
constexpr quint16 kProtoId = 0;
constexpr quint8 kMaxRegisters = 125;
// 寄存器区大小: 覆盖信捷XD5契约地址(最高约49416), 用65536整区
constexpr int kRegAreaSize = 65536;
}

ModbusTcpSlave::ModbusTcpSlave(QObject* parent)
    : QObject(parent)
{
    m_holding.fill(0, kRegAreaSize);
    m_input.fill(0, kRegAreaSize);
    m_coils.fill(false, kRegAreaSize);
    m_discrete.fill(false, kRegAreaSize);

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &ModbusTcpSlave::onNewConnection);

    m_behaviorTimer = new QTimer(this);
    m_behaviorTimer->setInterval(1000);
    connect(m_behaviorTimer, &QTimer::timeout,
            this, &ModbusTcpSlave::onBehaviorTick);
}

ModbusTcpSlave::~ModbusTcpSlave()
{
    stop();
}

bool ModbusTcpSlave::start(const QString& bindAddr, quint16 port, QString* err) {
    if (isRunning()) return true;
    if (!m_server->listen(QHostAddress(bindAddr), port)) {
        if (err) *err = QString("监听 %1:%2 失败: %3")
                            .arg(bindAddr).arg(port).arg(m_server->errorString());
        return false;
    }
    m_tick = 0;
    m_behaviorTimer->start();
    VI_LOG_INFO(QString("PLC模拟器已启动: %1:%2").arg(bindAddr).arg(port));
    return true;
}

void ModbusTcpSlave::stop() {
    if (m_server->isListening()) {
        m_server->close();
        VI_LOG_INFO("PLC模拟器已停止");
    }
    for (QTcpSocket* c : m_clients) {
        c->disconnectFromHost();
        c->deleteLater();
    }
    m_clients.clear();
    m_behaviorTimer->stop();
}

bool ModbusTcpSlave::isRunning() const {
    return m_server->isListening();
}

int ModbusTcpSlave::connectionCount() const {
    return m_clients.size();
}

QVector<quint16> ModbusTcpSlave::holdingSnapshot(int addr, int count) const {
    QMutexLocker locker(&m_mutex);
    QVector<quint16> out;
    for (int i = 0; i < count && addr + i < m_holding.size(); ++i)
        out.append(m_holding[addr + i]);
    return out;
}

void ModbusTcpSlave::setHolding(int addr, quint16 value) {
    QMutexLocker locker(&m_mutex);
    if (addr >= 0 && addr < m_holding.size()) m_holding[addr] = value;
}

quint16 ModbusTcpSlave::holding(int addr) const {
    QMutexLocker locker(&m_mutex);
    return (addr >= 0 && addr < m_holding.size()) ? m_holding[addr] : 0;
}

bool ModbusTcpSlave::coil(int addr) const {
    QMutexLocker locker(&m_mutex);
    return (addr >= 0 && addr < m_coils.size()) ? m_coils[addr] : false;
}

void ModbusTcpSlave::setCoil(int addr, bool on) {
    QMutexLocker locker(&m_mutex);
    if (addr >= 0 && addr < m_coils.size()) m_coils[addr] = on;
}

void ModbusTcpSlave::onBehaviorTick() {
    ++m_tick;
    {
        QMutexLocker locker(&m_mutex);
        if (m_autoIncAddr >= 0 && m_autoIncAddr < m_holding.size())
            ++m_holding[m_autoIncAddr];
        if (m_sineAddr >= 0 && m_sineAddr < m_holding.size()) {
            const double v = m_sineAmplitude *
                std::sin(2.0 * M_PI * double(m_tick % m_sinePeriod) / double(m_sinePeriod));
            m_input[m_sineAddr] = quint16(int(v) + 32768);  // 偏移到无符号中段
        }
    }
}

void ModbusTcpSlave::onNewConnection() {
    while (QTcpSocket* client = m_server->nextPendingConnection()) {
        m_clients.append(client);
        connect(client, &QTcpSocket::readyRead,
                this, &ModbusTcpSlave::onClientReadyRead);
        connect(client, &QTcpSocket::disconnected,
                this, &ModbusTcpSlave::onClientDisconnected);
        emit clientConnected(client->peerAddress().toString());
        VI_LOG_INFO("PLC模拟器: 客户端接入 " + client->peerAddress().toString());
    }
}

void ModbusTcpSlave::onClientReadyRead() {
    auto* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    // 简易粘包处理: 按MBAP帧切分
    static QMap<QTcpSocket*, QByteArray> buffers;
    QByteArray& buf = buffers[client];
    buf += client->readAll();

    while (buf.size() >= 7) {
        const quint16 rxLen = quint8(buf[4]) << 8 | quint8(buf[5]);
        const int total = 6 + rxLen;
        if (buf.size() < total) break;

        VI_LOG_DEBUG(QString("PLC模拟器收到帧: %1").arg(QString::fromLatin1(buf.left(total).toHex())));
        // 校验协议ID
        const quint16 proto = quint8(buf[2]) << 8 | quint8(buf[3]);
        if (proto == kProtoId) {
            client->setProperty("txEcho", buf.mid(0, 2));   // 事务ID原样回显
            const QByteArray pdu = buf.mid(7, rxLen - 1);
            if (!pdu.isEmpty())
                handlePdu(client, pdu);
        }
        buf.remove(0, total);
    }
}

void ModbusTcpSlave::onClientDisconnected() {
    auto* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    m_clients.removeAll(client);
    client->deleteLater();
    emit clientDisconnected();
}

void ModbusTcpSlave::handlePdu(QTcpSocket* client, const QByteArray& pdu) {
    const quint8 fn = quint8(pdu[0]);
    QByteArray resp;

    auto readU16 = [&](int off) -> int {
        if (off + 1 >= pdu.size()) return -1;
        return int(quint8(pdu[off]) << 8 | quint8(pdu[off + 1]));
    };

    QMutexLocker locker(&m_mutex);

    switch (fn) {
        case 0x03:   // 读保持寄存器
        case 0x04: { // 读输入寄存器
            const int addr = readU16(1), count = readU16(3);
            if (addr < 0 || count < 1 || count > kMaxRegisters || addr + count > kRegAreaSize) {
                resp = QByteArray(1, char(fn | 0x80)).append(char(2));
                break;
            }
            resp.append(char(fn));
            resp.append(char(count * 2));
            for (int i = 0; i < count; ++i) {
                const quint16 v = (fn == 0x03) ? m_holding[addr + i] : m_input[addr + i];
                resp.append(char(v >> 8)); resp.append(char(v & 0xFF));
            }
            break;
        }
        case 0x01:   // 读线圈
        case 0x02: { // 读离散输入
            const int addr = readU16(1), count = readU16(3);
            if (addr < 0 || count < 1 || count > kMaxRegisters * 8 || addr + count > kRegAreaSize) {
                resp = QByteArray(1, char(fn | 0x80)).append(char(2));
                break;
            }
            const int byteCount = (count + 7) / 8;
            resp.append(char(fn));
            resp.append(char(byteCount));
            QByteArray packed(byteCount, char(0));
            for (int i = 0; i < count; ++i) {
                const bool bit = (fn == 0x01) ? m_coils[addr + i] : m_discrete[addr + i];
                if (bit) packed[i / 8] = char(quint8(packed[i / 8]) | (1 << (i % 8)));
            }
            resp.append(packed);
            break;
        }
        case 0x06: { // 写单寄存器
            const int addr = readU16(1), value = readU16(3);
            if (addr < 0 || addr >= kRegAreaSize) {
                resp = QByteArray(1, char(0x86)).append(char(2));
                break;
            }
            m_holding[addr] = quint16(value);
            if (addr < m_input.size()) m_input[addr] = quint16(value);  // 输入区映射
            resp = pdu;  // 原样回显
            locker.unlock();
            emit registerWritten(addr, quint16(value));
            locker.relock();
            VI_LOG_INFO(QString("PLC模拟器: 主站写 R%1 = %2").arg(addr).arg(value));
            break;
        }
        case 0x10: { // 写多寄存器
            const int addr = readU16(1), count = readU16(3);
            const int byteCount = pdu.size() > 6 ? quint8(pdu[5]) : 0;
            if (addr < 0 || count < 1 || count > kMaxRegisters ||
                addr + count > kRegAreaSize || byteCount != count * 2) {
                resp = QByteArray(1, char(0x90)).append(char(2));
                break;
            }
            for (int i = 0; i < count; ++i) {
                const quint16 v = quint16(quint8(pdu[6 + i * 2]) << 8 | quint8(pdu[7 + i * 2]));
                m_holding[addr + i] = v;
                if (addr + i < m_input.size()) m_input[addr + i] = v;
            }
            resp.append(char(0x10));
            resp.append(char((addr >> 8) & 0xFF)); resp.append(char(addr & 0xFF));
            resp.append(char((count >> 8) & 0xFF)); resp.append(char(count & 0xFF));
            locker.unlock();
            for (int i = 0; i < count; ++i)
                emit registerWritten(addr + i,
                    quint16(quint8(pdu[6 + i * 2]) << 8 | quint8(pdu[7 + i * 2])));
            VI_LOG_INFO(QString("PLC模拟器: 主站写 R%1 起 %2 个寄存器").arg(addr).arg(count));
            locker.relock();
            break;
        }
        case 0x05: { // 写单线圈
            const int addr = readU16(1), value = readU16(3);
            if (addr < 0 || addr >= kRegAreaSize) {
                resp = QByteArray(1, char(0x85)).append(char(2));
                break;
            }
            const bool on = (value == 0xFF00);
            m_coils[addr] = on;
            if (addr < m_discrete.size()) m_discrete[addr] = on;
            resp = pdu;
            locker.unlock();
            emit coilWritten(addr, on);
            VI_LOG_INFO(QString("PLC模拟器: 主站写 C%1 = %2").arg(addr).arg(on ? "ON" : "OFF"));
            locker.relock();
            break;
        }
        case 0x0F: { // 写多线圈
            const int addr = readU16(1), count = readU16(3);
            const int byteCount = pdu.size() > 6 ? quint8(pdu[5]) : 0;
            if (addr < 0 || count < 1 || addr + count > kRegAreaSize ||
                byteCount != (count + 7) / 8) {
                resp = QByteArray(1, char(0x8F)).append(char(2));
                break;
            }
            for (int i = 0; i < count; ++i) {
                const bool bit = (quint8(pdu[5 + i / 8]) >> (i % 8)) & 1;
                m_coils[addr + i] = bit;
                if (addr + i < m_discrete.size()) m_discrete[addr + i] = bit;
            }
            resp.append(char(0x0F));
            resp.append(char((addr >> 8) & 0xFF)); resp.append(char(addr & 0xFF));
            resp.append(char((count >> 8) & 0xFF)); resp.append(char(count & 0xFF));
            break;
        }
        default:
            resp = QByteArray(1, char(fn | 0x80)).append(char(1));  // 非法功能
    }

    // 回复
    QByteArray frame;
    const quint16 rxLen = quint16(resp.size() + 1);
    frame.append(client->property("txEcho").toByteArray());
    frame.append(char(kProtoId >> 8)); frame.append(char(kProtoId & 0xFF));
    frame.append(char(rxLen >> 8)); frame.append(char(rxLen & 0xFF));
    frame.append(char(1));  // 单元ID固定1
    frame.append(resp);
    client->write(frame);
}

} // namespace VisionInspector
