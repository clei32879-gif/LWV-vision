/**
 * @file ModbusTcpMaster.h
 * @brief Modbus TCP 主站 (客户端) — 自研实现, 仅依赖 Qt Network
 *
 * MBAP + PDU 协议, 支持功能码:
 *   0x01 读线圈  0x02 读离散输入  0x03 读保持寄存器  0x04 读输入寄存器
 *   0x05 写单线圈  0x06 写单寄存器  0x0F 写多线圈  0x10 写多寄存器
 *
 * 连接池: 相同 host:port 复用一个连接 (多个工具节点共用一次TCP握手)。
 * 阻塞式API: 专为流程工作线程设计 (工具的execute在工作线程调用)。
 */

#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QVector>
#include <QMutex>
#include <QMap>

namespace VisionInspector {

class ModbusTcpMaster : public QObject {
    Q_OBJECT

public:
    /** 获取(或创建)到 host:port 的共享连接 */
    static ModbusTcpMaster* acquire(const QString& host, quint16 port, QString* err = nullptr);
    /** 断开并释放所有池中连接 (程序退出时调用) */
    static void releaseAll();

    /** 确保已连接 (未连则连, 失败返回false并填err) */
    bool ensureConnected(QString* err = nullptr);
    void disconnectFromDevice();

    /** 读保持/输入寄存器 (function=3/4), count<=125 */
    QVector<quint16> readRegisters(int function, int addr, int count,
                                   int slaveId, QString* err = nullptr);
    /** 读线圈/离散输入 (function=1/2), count<=2000 */
    QVector<bool> readBits(int function, int addr, int count,
                           int slaveId, QString* err = nullptr);
    bool writeRegister(int addr, quint16 value, int slaveId, QString* err = nullptr);
    bool writeRegisters(int addr, const QVector<quint16>& values, int slaveId, QString* err = nullptr);
    bool writeCoil(int addr, bool on, int slaveId, QString* err = nullptr);
    bool writeCoils(int addr, const QVector<bool>& bits, int slaveId, QString* err = nullptr);

    QString endpoint() const { return m_host + ":" + QString::number(m_port); }
    bool isConnected() const;

private:
    explicit ModbusTcpMaster(const QString& host, quint16 port, QObject* parent = nullptr);
    /** 发送请求并等待响应PDU, 返回响应(不含MBAP); 失败返回空并填err */
    QByteArray transact(quint8 function, const QByteArray& pdu, int slaveId, QString* err);

    QString m_host;
    quint16 m_port;
    QTcpSocket m_socket;
    quint16 m_txId = 0;
    QMutex m_mutex;   // 同一连接可能被不同线程的工具使用

    static QMutex s_poolMutex;
    static QMap<QString, ModbusTcpMaster*> s_pool;
};

} // namespace VisionInspector
