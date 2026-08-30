/**
 * @file ModbusRtuMaster.cpp
 * @brief Modbus RTU 主站实现 (串口 RS232/485)
 */

#include "ModbusRtuMaster.h"
#include "../utils/Logger.h"
#include <QDateTime>

namespace VisionInspector {

QMutex ModbusRtuMaster::s_poolMutex;
QMap<QString, ModbusRtuMaster*> ModbusRtuMaster::s_pool;

ModbusRtuMaster* ModbusRtuMaster::acquire(const SerialParams& params, QString* err) {
    const QString key = "RTU:" + params.portName + ":" + QString::number(params.baudRate)
                      + ":" + QString::number(params.dataBits) + ":" + QString::number(params.stopBits)
                      + ":" + params.parity;
    QMutexLocker locker(&s_poolMutex);
    auto it = s_pool.find(key);
    if (it != s_pool.end())
        return it.value();
    auto* master = new ModbusRtuMaster(params);
    s_pool.insert(key, master);
    if (err) err->clear();
    return master;
}

void ModbusRtuMaster::releaseAll() {
    QMutexLocker locker(&s_poolMutex);
    for (auto it = s_pool.begin(); it != s_pool.end(); ++it) {
        it.value()->disconnectFromDevice();
        it.value()->deleteLater();
    }
    s_pool.clear();
}

ModbusRtuMaster::ModbusRtuMaster(const SerialParams& params, QObject* parent)
    : QObject(parent), m_params(params)
{
    m_device = &m_serial;
}

ModbusRtuMaster::~ModbusRtuMaster() = default;

void ModbusRtuMaster::setDeviceForTest(QIODevice* device) {
    m_device = device;
    m_ownsSerial = (device == nullptr);
}

bool ModbusRtuMaster::isConnected() const {
    return m_serial.isOpen();
}

QString ModbusRtuMaster::endpoint() const {
    return m_params.portName + "@" + QString::number(m_params.baudRate);
}

bool ModbusRtuMaster::ensureConnected(QString* err) {
    if (m_device && m_device->isOpen()) return true;
    if (m_ownsSerial) {
        m_serial.setPortName(m_params.portName);
        m_serial.setBaudRate(m_params.baudRate);
        m_serial.setDataBits(QSerialPort::DataBits(m_params.dataBits));
        m_serial.setStopBits(m_params.stopBits == 2 ? QSerialPort::TwoStop
                                                    : QSerialPort::OneStop);
        m_serial.setParity(m_params.parity == QStringLiteral("偶") ? QSerialPort::EvenParity
                          : m_params.parity == QStringLiteral("奇") ? QSerialPort::OddParity
                          : QSerialPort::NoParity);
        m_serial.setFlowControl(QSerialPort::NoFlowControl);
        if (!m_serial.open(QIODevice::ReadWrite)) {
            if (err) *err = QString("打开串口 %1 失败: %2")
                                .arg(m_params.portName)
                                .arg(m_serial.errorString());
            return false;
        }
        VI_LOG_INFO("Modbus RTU 已打开串口: " + endpoint());
    }
    return true;
}

void ModbusRtuMaster::disconnectFromDevice() {
    if (m_ownsSerial && m_serial.isOpen())
        m_serial.close();
}

QByteArray ModbusRtuMaster::buildFrame(int slaveId, quint8 function, const QByteArray& data) {
    QByteArray f;
    f.append(char(slaveId & 0xFF));
    f.append(char(function));
    f.append(data);
    return f;
}

QByteArray ModbusRtuMaster::appendCrc(const QByteArray& frame) {
    quint16 crc = 0xFFFF;
    for (int i = 0; i < frame.size(); ++i) {
        crc ^= quint8(frame[i]);
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    QByteArray out = frame;
    out.append(char(crc & 0xFF));       // CRC低字节在前
    out.append(char((crc >> 8) & 0xFF));
    return out;
}

QByteArray ModbusRtuMaster::stripCrc(const QByteArray& frame) {
    if (frame.size() < 4) return {};
    const QByteArray withoutCrc = frame.left(frame.size() - 2);
    const QByteArray expected = appendCrc(withoutCrc);
    if (expected.right(2) != frame.right(2)) return {};  // CRC不匹配
    return withoutCrc;
}

int ModbusRtuMaster::expectedRespLen(quint8 function, quint8 byteCount) {
    // 返回整个RTU帧期望字节数(含从站+功能码+数据+CRC2)
    switch (function) {
        case 0x01: case 0x02: case 0x03: case 0x04:
            return 3 + byteCount + 2;      // 从站+功能码+字节数 + 数据 + CRC
        case 0x05: case 0x06:
            return 8;                       // 从站+功能码+地址2+值2+CRC2
        case 0x0F: case 0x10:
            return 8;                       // 从站+功能码+地址2+数量2+CRC2
        default:
            return -1;
    }
}

QByteArray ModbusRtuMaster::transact(quint8 function, const QByteArray& data,
                                     int slaveId, QString* err) {
    QMutexLocker locker(&m_mutex);
    if (!m_device) { if (err) *err = "无通讯设备"; return {}; }
    if (!ensureConnected(err)) return {};

    const QByteArray frame = appendCrc(buildFrame(slaveId, function, data));
    m_device->readAll();  // 丢弃残留数据

    qint64 written = m_device->write(frame);
    if (!m_device->waitForBytesWritten(m_params.timeoutMs) || written != frame.size()) {
        if (err) *err = "发送失败";
        return {};
    }

    // 读取响应: 先读2字节(从站+功能码)推断长度, 再收满
    QByteArray resp;
    const int deadline = m_params.timeoutMs;
    const qint64 start = QDateTime::currentMSecsSinceEpoch();
    while ((QDateTime::currentMSecsSinceEpoch() - start) < deadline) {
        if (m_device->waitForReadyRead(200)) {
            resp += m_device->readAll();
            if (resp.size() >= 2) {
                const quint8 rfn = quint8(resp[1]);
                // 异常响应: 功能码最高位置1
                int expect = (rfn & 0x80) ? 5 : expectedRespLen(rfn, resp.size() > 2 ? quint8(resp[2]) : 0);
                if (expect > 0 && resp.size() >= expect) {
                    // 丢弃可能粘包的多余字节
                    if (resp.size() > expect) resp.truncate(expect);
                    break;
                }
            }
        }
        if (!m_device->isOpen()) {
            if (err) *err = "连接中断";
            return {};
        }
    }

    if (resp.size() < 2) {
        if (err) *err = "响应超时";
        return {};
    }

    // 从站号校验
    if (quint8(resp[0]) != slaveId) {
        if (err) *err = "从站ID不匹配";
        return {};
    }

    const QByteArray noCrc = stripCrc(resp);
    if (noCrc.isEmpty()) {
        if (err) *err = "CRC校验失败";
        return {};
    }

    // 异常响应
    const quint8 rfn = quint8(noCrc[1]);
    if (rfn & 0x80) {
        const quint8 code = noCrc.size() > 2 ? quint8(noCrc[2]) : 0;
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
    return noCrc;
}

QVector<quint16> ModbusRtuMaster::readRegisters(int function, int addr, int count,
                                                int slaveId, QString* err) {
    if (count < 1 || count > 125) { if (err) *err = "数量须在1~125"; return {}; }
    QByteArray data;
    data.append(char((addr >> 8) & 0xFF));  data.append(char(addr & 0xFF));
    data.append(char((count >> 8) & 0xFF)); data.append(char(count & 0xFF));

    const QByteArray resp = transact(quint8(function), data, slaveId, err);
    if (resp.isEmpty()) return {};

    if (resp.size() < 3 || quint8(resp[2]) != resp.size() - 3) {
        if (err) *err = "响应长度不符";
        return {};
    }
    QVector<quint16> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int o = 3 + i * 2;
        if (o + 1 >= resp.size()) { if (err) *err = "响应数据不足"; return {}; }
        out.append(quint16(quint8(resp[o]) << 8 | quint8(resp[o + 1])));
    }
    return out;
}

QVector<bool> ModbusRtuMaster::readBits(int function, int addr, int count,
                                        int slaveId, QString* err) {
    if (count < 1 || count > 2000) { if (err) *err = "数量须在1~2000"; return {}; }
    QByteArray data;
    data.append(char((addr >> 8) & 0xFF));  data.append(char(addr & 0xFF));
    data.append(char((count >> 8) & 0xFF)); data.append(char(count & 0xFF));

    const QByteArray resp = transact(quint8(function), data, slaveId, err);
    if (resp.isEmpty()) return {};

    QVector<bool> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int byteIdx = 3 + i / 8;
        const int bitIdx = i % 8;
        if (byteIdx >= resp.size()) { if (err) *err = "响应数据不足"; return {}; }
        out.append((quint8(resp[byteIdx]) >> bitIdx) & 1);
    }
    return out;
}

bool ModbusRtuMaster::writeRegister(int addr, quint16 value, int slaveId, QString* err) {
    QByteArray data;
    data.append(char((addr >> 8) & 0xFF)); data.append(char(addr & 0xFF));
    data.append(char((value >> 8) & 0xFF)); data.append(char(value & 0xFF));
    return !transact(0x06, data, slaveId, err).isEmpty();
}

bool ModbusRtuMaster::writeRegisters(int addr, const QVector<quint16>& values,
                                     int slaveId, QString* err) {
    if (values.isEmpty() || values.size() > 123) {
        if (err) *err = "数量须在1~123"; return false;
    }
    QByteArray data;
    data.append(char((addr >> 8) & 0xFF)); data.append(char(addr & 0xFF));
    data.append(char((values.size() >> 8) & 0xFF)); data.append(char(values.size() & 0xFF));
    data.append(char(values.size() * 2));
    for (quint16 v : values) {
        data.append(char(v >> 8)); data.append(char(v & 0xFF));
    }
    return !transact(0x10, data, slaveId, err).isEmpty();
}

bool ModbusRtuMaster::writeCoil(int addr, bool on, int slaveId, QString* err) {
    QByteArray data;
    data.append(char((addr >> 8) & 0xFF)); data.append(char(addr & 0xFF));
    const quint16 val = on ? 0xFF00 : 0x0000;
    data.append(char((val >> 8) & 0xFF)); data.append(char(val & 0xFF));
    return !transact(0x05, data, slaveId, err).isEmpty();
}

bool ModbusRtuMaster::writeCoils(int addr, const QVector<bool>& bits,
                                 int slaveId, QString* err) {
    if (bits.isEmpty() || bits.size() > 1968) {
        if (err) *err = "数量须在1~1968"; return false;
    }
    QByteArray data;
    data.append(char((addr >> 8) & 0xFF)); data.append(char(addr & 0xFF));
    data.append(char((bits.size() >> 8) & 0xFF)); data.append(char(bits.size() & 0xFF));
    const int byteCount = (bits.size() + 7) / 8;
    data.append(char(byteCount));
    QByteArray packed(byteCount, char(0));
    for (int i = 0; i < bits.size(); ++i) {
        if (bits[i]) packed[i / 8] = char(quint8(packed[i / 8]) | (1 << (i % 8)));
    }
    data.append(packed);
    return !transact(0x0F, data, slaveId, err).isEmpty();
}

} // namespace VisionInspector
