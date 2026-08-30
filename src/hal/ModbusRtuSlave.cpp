/**
 * @file ModbusRtuSlave.cpp
 * @brief Modbus RTU 从站模拟器实现
 */

#include "ModbusRtuSlave.h"
#include "../utils/Logger.h"

namespace VisionInspector {

namespace {
// 寄存器区大小: 覆盖信捷XD5契约地址(最高约49416), 用65536整区
constexpr int kRegAreaSize = 65536;

quint16 crc16(const QByteArray& data) {
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= quint8(data[i]);
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}
} // namespace

ModbusRtuSlave::ModbusRtuSlave(QObject* parent)
    : QObject(parent)
{
    m_holding.fill(0, kRegAreaSize);
    m_input.fill(0, kRegAreaSize);
    m_coils.fill(false, kRegAreaSize);
    m_discrete.fill(false, kRegAreaSize);
    m_device = &m_serial;

    m_behaviorTimer = new QTimer(this);
    m_behaviorTimer->setInterval(1000);
    connect(m_behaviorTimer, &QTimer::timeout, this, &ModbusRtuSlave::onBehaviorTick);
}

ModbusRtuSlave::~ModbusRtuSlave() {
    stop();
}

void ModbusRtuSlave::setDeviceForTest(QIODevice* device) {
    stop();
    if (device) {
        m_device = device;
        m_ownsSerial = false;
        connect(device, &QIODevice::readyRead, this, &ModbusRtuSlave::onReadyRead,
                Qt::UniqueConnection);
    } else {
        m_device = &m_serial;
        m_ownsSerial = true;
    }
}

bool ModbusRtuSlave::start(const QString& portName, int baudRate,
                           int dataBits, int stopBits, const QString& parity,
                           QString* err) {
    stop();
    m_serial.setPortName(portName);
    m_serial.setBaudRate(baudRate);
    m_serial.setDataBits(QSerialPort::DataBits(dataBits));
    m_serial.setStopBits(stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
    m_serial.setParity(parity == QStringLiteral("偶") ? QSerialPort::EvenParity
                       : parity == QStringLiteral("奇") ? QSerialPort::OddParity
                       : QSerialPort::NoParity);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);
    if (!m_serial.open(QIODevice::ReadWrite)) {
        if (err) *err = QString("打开串口 %1 失败: %2").arg(portName).arg(m_serial.errorString());
        return false;
    }
    connect(&m_serial, &QIODevice::readyRead, this, &ModbusRtuSlave::onReadyRead,
            Qt::UniqueConnection);
    m_behaviorTimer->start();
    VI_LOG_INFO("Modbus RTU 从站已监听串口: " + portName);
    return true;
}

void ModbusRtuSlave::stop() {
    m_behaviorTimer->stop();
    if (m_ownsSerial && m_serial.isOpen()) {
        disconnect(&m_serial, &QIODevice::readyRead, this, &ModbusRtuSlave::onReadyRead);
        m_serial.close();
    }
    m_rxBuf.clear();
}

bool ModbusRtuSlave::isRunning() const {
    return m_device && m_device->isOpen();
}

QVector<quint16> ModbusRtuSlave::holdingSnapshot(int addr, int count) const {
    QMutexLocker locker(&m_mutex);
    QVector<quint16> out;
    for (int i = 0; i < count && addr + i < m_holding.size(); ++i)
        out.append(m_holding[addr + i]);
    return out;
}

void ModbusRtuSlave::setHolding(int addr, quint16 value) {
    QMutexLocker locker(&m_mutex);
    if (addr >= 0 && addr < m_holding.size()) m_holding[addr] = value;
}

quint16 ModbusRtuSlave::holding(int addr) const {
    QMutexLocker locker(&m_mutex);
    return (addr >= 0 && addr < m_holding.size()) ? m_holding[addr] : 0;
}

bool ModbusRtuSlave::coil(int addr) const {
    QMutexLocker locker(&m_mutex);
    return (addr >= 0 && addr < m_coils.size()) ? m_coils[addr] : false;
}

void ModbusRtuSlave::setCoil(int addr, bool on) {
    QMutexLocker locker(&m_mutex);
    if (addr >= 0 && addr < m_coils.size()) m_coils[addr] = on;
}

void ModbusRtuSlave::sendFrame(const QByteArray& pdu) {
    QByteArray frame = pdu;
    const quint16 crc = crc16(pdu);
    frame.append(char(crc & 0xFF));
    frame.append(char((crc >> 8) & 0xFF));
    if (m_device && m_device->isOpen()) {
        m_device->write(frame);
        m_device->waitForBytesWritten(200);
    }
}

void ModbusRtuSlave::onReadyRead() {
    if (!m_device) return;
    m_rxBuf += m_device->readAll();

    // 尝试按RTU帧结构解析: 可能一次收到多帧或半帧, 逐帧处理
    while (m_rxBuf.size() >= 4) {
        const quint8 slaveId = quint8(m_rxBuf[0]);
        const quint8 fn = quint8(m_rxBuf[1]);
        int frameLen = -1;
        if (fn <= 0x04) {
            frameLen = 8;                                  // 读类: 从站+功能码+地址2+数量2+CRC2
        } else if (fn == 0x05 || fn == 0x06) {
            frameLen = 8;
        } else if (fn == 0x0F || fn == 0x10) {
            if (m_rxBuf.size() >= 7) {
                const int byteCount = quint8(m_rxBuf[6]);
                frameLen = 7 + byteCount + 2;              // 从站+功能码+地址2+数量2+字节数+数据+CRC2
            } else {
                break;                                     // 等待更多字节
            }
        } else {
            // 未知功能码: 丢弃首个字节重试
            m_rxBuf.remove(0, 1);
            continue;
        }

        if (m_rxBuf.size() < frameLen) break;              // 半帧, 等下一次

        const QByteArray frame = m_rxBuf.left(frameLen);
        m_rxBuf.remove(0, frameLen);

        // CRC校验
        const QByteArray pdu = frame.left(frameLen - 2);
        const quint16 calc = crc16(pdu);
        const quint16 recv = quint8(frame[frameLen - 2]) | (quint16(quint8(frame[frameLen - 1])) << 8);
        if (calc != recv) {
            VI_LOG_INFO("Modbus RTU 从站: CRC不匹配, 丢弃该帧");
            continue;
        }

        // 忽略不是发给我们的帧
        if (slaveId != 0 && slaveId != 0xFF) {
            // 支持单元ID=0(广播)与0xFF(任意); 其余只响应匹配从站
            // 模拟器固定监听从站1, 简单处理: 非1则丢弃
            if (slaveId != 1) continue;
        }

        const QByteArray resp = handlePdu(pdu);
        if (!resp.isEmpty()) sendFrame(resp);
    }
}

QByteArray ModbusRtuSlave::handlePdu(const QByteArray& pdu) {
    const quint8 fn = quint8(pdu[1]);
    if (pdu.size() < 2) return {};

    QByteArray resp;
    resp.append(pdu[0]);   // 从站号回显
    resp.append(char(fn));

    switch (fn) {
        case 0x01: case 0x02: {
            if (pdu.size() < 6) return exception(pdu, 3);
            const int addr = quint8(pdu[2]) << 8 | quint8(pdu[3]);
            const int count = quint8(pdu[4]) << 8 | quint8(pdu[5]);
            if (count < 1 || count > 2000 || addr + count > kRegAreaSize) return exception(pdu, 2);
            QVector<bool> bits;
            QMutexLocker locker(&m_mutex);
            for (int i = 0; i < count; ++i)
                bits.append(fn == 0x01 ? m_coils[addr + i] : m_discrete[addr + i]);
            locker.unlock();
            const int byteCount = (count + 7) / 8;
            resp.append(char(byteCount));
            QByteArray packed(byteCount, char(0));
            for (int i = 0; i < count; ++i)
                if (bits[i]) packed[i / 8] = char(quint8(packed[i / 8]) | (1 << (i % 8)));
            resp.append(packed);
            return resp;
        }
        case 0x03: case 0x04: {
            if (pdu.size() < 6) return exception(pdu, 3);
            const int addr = quint8(pdu[2]) << 8 | quint8(pdu[3]);
            const int count = quint8(pdu[4]) << 8 | quint8(pdu[5]);
            if (count < 1 || count > 125 || addr + count > kRegAreaSize) return exception(pdu, 2);
            resp.append(char(count * 2));
            QMutexLocker locker(&m_mutex);
            for (int i = 0; i < count; ++i) {
                const quint16 v = (fn == 0x03) ? m_holding[addr + i] : m_input[addr + i];
                resp.append(char(v >> 8)); resp.append(char(v & 0xFF));
            }
            return resp;
        }
        case 0x05: {
            if (pdu.size() < 6) return exception(pdu, 3);
            const int addr = quint8(pdu[2]) << 8 | quint8(pdu[3]);
            const bool on = quint8(pdu[4]) == 0xFF;
            if (addr >= kRegAreaSize) return exception(pdu, 2);
            {
                QMutexLocker locker(&m_mutex);
                m_coils[addr] = on;
            }
            emit coilWritten(addr, on);
            return pdu;   // 回显原请求
        }
        case 0x06: {
            if (pdu.size() < 6) return exception(pdu, 3);
            const int addr = quint8(pdu[2]) << 8 | quint8(pdu[3]);
            const quint16 v = quint8(pdu[4]) << 8 | quint8(pdu[5]);
            if (addr >= kRegAreaSize) return exception(pdu, 2);
            {
                QMutexLocker locker(&m_mutex);
                m_holding[addr] = v;
                if (addr < m_input.size()) m_input[addr] = v;  // 输入区同步映射
            }
            emit registerWritten(addr, v);
            return pdu;
        }
        case 0x0F: {
            if (pdu.size() < 7) return exception(pdu, 3);
            const int addr = quint8(pdu[2]) << 8 | quint8(pdu[3]);
            const int count = quint8(pdu[4]) << 8 | quint8(pdu[5]);
            if (count < 1 || count > 1968 || addr + count > kRegAreaSize) return exception(pdu, 2);
            const int byteCount = quint8(pdu[6]);
            if (pdu.size() < 7 + byteCount) return exception(pdu, 3);
            {
                QMutexLocker locker(&m_mutex);
                for (int i = 0; i < count; ++i) {
                    const int byteIdx = 7 + i / 8;
                    const int bitIdx = i % 8;
                    m_coils[addr + i] = (quint8(pdu[byteIdx]) >> bitIdx) & 1;
                }
            }
            // 响应: 从站+功能码+地址2+数量2 (不含数据)
            QByteArray r;
            r.append(pdu[0]); r.append(char(0x0F));
            r.append(pdu[2]); r.append(pdu[3]);
            r.append(pdu[4]); r.append(pdu[5]);
            return r;
        }
        case 0x10: {
            if (pdu.size() < 7) return exception(pdu, 3);
            const int addr = quint8(pdu[2]) << 8 | quint8(pdu[3]);
            const int count = quint8(pdu[4]) << 8 | quint8(pdu[5]);
            if (count < 1 || count > 123 || addr + count > kRegAreaSize) return exception(pdu, 2);
            const int byteCount = quint8(pdu[6]);
            if (pdu.size() < 7 + byteCount) return exception(pdu, 3);
            {
                QMutexLocker locker(&m_mutex);
                for (int i = 0; i < count; ++i) {
                    const int o = 7 + i * 2;
                    const quint16 v = quint8(pdu[o]) << 8 | quint8(pdu[o + 1]);
                    m_holding[addr + i] = v;
                    if (addr + i < m_input.size()) m_input[addr + i] = v;
                }
            }
            for (int i = 0; i < count; ++i)
                emit registerWritten(addr + i, holding(addr + i));
            QByteArray r;
            r.append(pdu[0]); r.append(char(0x10));
            r.append(pdu[2]); r.append(pdu[3]);
            r.append(pdu[4]); r.append(pdu[5]);
            return r;
        }
        default:
            return exception(pdu, 1);
    }
}

QByteArray ModbusRtuSlave::exception(const QByteArray& pdu, quint8 code) {
    QByteArray r;
    r.append(pdu[0]);
    r.append(char(quint8(pdu[1]) | 0x80));
    r.append(char(code));
    return r;
}

void ModbusRtuSlave::onBehaviorTick() {
    ++m_tick;
    QMutexLocker locker(&m_mutex);
    if (m_autoIncAddr >= 0 && m_autoIncAddr < m_holding.size())
        ++m_holding[m_autoIncAddr];
    if (m_sineAddr >= 0 && m_sineAddr < m_holding.size()) {
        const double ang = 2.0 * 3.14159265358979 * double(m_tick) / double(m_sinePeriod);
        m_holding[m_sineAddr] = quint16(double(m_sineAmplitude) * (1.0 + qSin(ang)) * 0.5);
    }
}

} // namespace VisionInspector
