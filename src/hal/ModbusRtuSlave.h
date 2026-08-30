/**
 * @file ModbusRtuSlave.h
 * @brief Modbus RTU 从站模拟器 — 无真实PLC/串口时的联调/演示工具
 *
 * 与 ModbusTcpSlave 同构, 但走串口 RTU 帧 (地址+功能码+数据+CRC16)。
 * 默认持有 QSerialPort; 测试可注入一对互连的 QLocalSocket 充当虚拟串口线,
 * 与 ModbusRtuMaster::setDeviceForTest 配对, 实现无硬件全流程联测。
 *
 * 完整模拟保持寄存器/输入寄存器/线圈/离散输入四个区, 可选自动行为。
 */

#pragma once

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QVector>
#include <QMutex>
#include <QIODevice>

namespace VisionInspector {

class ModbusRtuSlave : public QObject {
    Q_OBJECT

public:
    explicit ModbusRtuSlave(QObject* parent = nullptr);
    ~ModbusRtuSlave() override;

    /** 打开串口并进入服务 (寄存器区大小默认4096) */
    bool start(const QString& portName, int baudRate = 9600,
               int dataBits = 8, int stopBits = 1, const QString& parity = QStringLiteral("无"),
               QString* err = nullptr);
    void stop();
    bool isRunning() const;

    /** 供测试: 注入自定义收发设备 (如 QLocalSocket 对). 由调用方持有生命周期. */
    void setDeviceForTest(QIODevice* device);

    // ---- 寄存器操作 (线程安全) ----
    QVector<quint16> holdingSnapshot(int addr, int count) const;
    void setHolding(int addr, quint16 value);
    quint16 holding(int addr) const;
    bool coil(int addr) const;
    void setCoil(int addr, bool on);

    // ---- 自动行为 ----
    void setAutoIncrement(int addr) { m_autoIncAddr = addr; }
    void setSineRegister(int addr, int amplitude = 1000, int periodSec = 10) {
        m_sineAddr = addr; m_sineAmplitude = amplitude; m_sinePeriod = periodSec;
    }

signals:
    void registerWritten(int addr, quint16 value);
    void coilWritten(int addr, bool on);

private slots:
    void onReadyRead();
    void onBehaviorTick();

private:
    QByteArray handlePdu(const QByteArray& pdu);  // 含从站+功能码的请求(已去CRC)
    QByteArray exception(const QByteArray& pdu, quint8 code);
    void sendFrame(const QByteArray& pdu);        // 组CRC后发送

    QSerialPort m_serial;
    QIODevice* m_device = nullptr;   // 默认指向 m_serial
    bool m_ownsSerial = true;
    QTimer* m_behaviorTimer = nullptr;
    QByteArray m_rxBuf;

    QVector<quint16> m_holding;
    QVector<quint16> m_input;
    QVector<bool> m_coils;
    QVector<bool> m_discrete;

    int m_autoIncAddr = -1;
    int m_sineAddr = -1;
    int m_sineAmplitude = 1000;
    int m_sinePeriod = 10;
    qint64 m_tick = 0;

    mutable QMutex m_mutex;
};

} // namespace VisionInspector
