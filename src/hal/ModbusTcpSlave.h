/**
 * @file ModbusTcpSlave.h
 * @brief Modbus TCP 从站模拟器 — 无真实PLC时的联调/演示工具
 *
 * 完整模拟保持寄存器/输入寄存器/线圈/离散输入四个区:
 *   - 主站可读写保持寄存器与线圈 (写入触发 registerWritten 信号)
 *   - 输入寄存器/离散输入默认映射保持寄存器/线圈 (方便测试读类功能码)
 *   - 可选自动行为: 指定寄存器自增(模拟产量计数)、正弦变化(模拟传感器)
 *
 * 用途:
 *   - 软件内启动后, Modbus读/写工具指向 127.0.0.1 即可全流程联调
 *   - 模拟信捷PLC的交互时序 (检测完成->写OK/NG->PLC吹气)
 */

#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVector>
#include <QMutex>

namespace VisionInspector {

class ModbusTcpSlave : public QObject {
    Q_OBJECT

public:
    explicit ModbusTcpSlave(QObject* parent = nullptr);
    ~ModbusTcpSlave() override;

    /** 启动监听 (寄存器区大小默认4096) */
    bool start(const QString& bindAddr, quint16 port, QString* err = nullptr);
    void stop();
    bool isRunning() const;

    // ---- 寄存器操作 (线程安全) ----
    QVector<quint16> holdingSnapshot(int addr, int count) const;
    void setHolding(int addr, quint16 value);
    quint16 holding(int addr) const;
    bool coil(int addr) const;
    void setCoil(int addr, bool on);

    // ---- 自动行为 ----
    /** 自增寄存器: 每秒+1 (模拟产量计数器), -1关闭 */
    void setAutoIncrement(int addr) { m_autoIncAddr = addr; }
    int autoIncrement() const { return m_autoIncAddr; }
    /** 正弦寄存器: 每秒按正弦变化 (模拟模拟量传感器), -1关闭 */
    void setSineRegister(int addr, int amplitude = 1000, int periodSec = 10) {
        m_sineAddr = addr; m_sineAmplitude = amplitude; m_sinePeriod = periodSec;
    }

    int connectionCount() const;

signals:
    /** 客户端写保持寄存器 (软件通过MB写数据工具写PLC时发出) */
    void registerWritten(int addr, quint16 value);
    /** 客户端写线圈 */
    void coilWritten(int addr, bool on);
    void clientConnected(const QString& peer);
    void clientDisconnected();

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onBehaviorTick();

private:
    void handlePdu(QTcpSocket* client, const QByteArray& pdu);
    QByteArray buildResponse(const QByteArray& reqPdu);

    QTcpServer* m_server = nullptr;
    QVector<QTcpSocket*> m_clients;
    QTimer* m_behaviorTimer = nullptr;   // 自动行为节拍(1s)

    // 寄存器区
    QVector<quint16> m_holding;
    QVector<quint16> m_input;
    QVector<bool> m_coils;
    QVector<bool> m_discrete;

    // 自动行为
    int m_autoIncAddr = -1;
    int m_sineAddr = -1;
    int m_sineAmplitude = 1000;
    int m_sinePeriod = 10;
    qint64 m_tick = 0;

    mutable QMutex m_mutex;
    quint16 m_txIdEcho = 0;
};

} // namespace VisionInspector
