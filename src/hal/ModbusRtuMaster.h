/**
 * @file ModbusRtuMaster.h
 * @brief Modbus RTU 主站 (串口 RS232/485) — 自研实现, 仅依赖 Qt SerialPort
 *
 * 用于信捷 XD5 等 PLC 的串口联动 (现场接线: PLC COM 口 → USB-RS232/485 转接 → 电脑)。
 * 支持功能码:
 *   0x01 读线圈  0x02 读离散输入  0x03 读保持寄存器  0x04 读输入寄存器
 *   0x05 写单线圈  0x06 写单寄存器  0x0F 写多线圈  0x10 写多寄存器
 *
 * 帧格式: [从站地址][功能码][数据][CRC16-LE]。
 * 连接池: 相同串口参数复用一个连接 (多个工具节点共用一次串口打开)。
 * 阻塞式API: 专为流程工作线程设计 (工具的execute在工作线程调用)。
 *
 * 可测性: 内部通过 QIODevice 收发字节, 默认持有 QSerialPort; 测试可注入
 * 一对互连的 QLocalSocket 充当"虚拟串口线", 无需真实硬件即可全流程联测。
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <QMap>
#include <QSerialPort>

namespace VisionInspector {

class ModbusRtuMaster : public QObject {
    Q_OBJECT

public:
    /** 串口参数 */
    struct SerialParams {
        QString portName = "COM1";
        int baudRate = 9600;
        int dataBits = 8;            // 5/6/7/8
        int stopBits = 1;            // 1/2
        QString parity = "无";        // 无/偶/奇
        int timeoutMs = 1000;
    };

    /** 获取(或创建)到指定串口参数的共享连接 */
    static ModbusRtuMaster* acquire(const SerialParams& params, QString* err = nullptr);
    /** 断开并释放所有池中连接 (程序退出时调用) */
    static void releaseAll();

    /** 打开串口 (未开则开, 失败返回false并填err) */
    bool ensureConnected(QString* err = nullptr);
    void disconnectFromDevice();
    bool isConnected() const;

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

    QString endpoint() const;

    /** 供测试: 注入自定义收发设备 (如 QLocalSocket 对). 由调用方持有device生命周期. */
    void setDeviceForTest(QIODevice* device);

    /** 直接构造(测试/独立使用); 常规路径请用 acquire() 共享连接 */
    explicit ModbusRtuMaster(const SerialParams& params, QObject* parent = nullptr);
    ~ModbusRtuMaster() override;

private:
    /** 组RTU帧(不含CRC) */
    static QByteArray buildFrame(int slaveId, quint8 function, const QByteArray& data);
    /** 追加CRC16-LE */
    static QByteArray appendCrc(const QByteArray& frame);
    /** 校验并提取CRC, 返回去掉CRC的帧; 失败返回空 */
    static QByteArray stripCrc(const QByteArray& frame);
    /** 发送请求帧并等待响应, 返回响应(不含CRC与从站校验); 失败返回空并填err */
    QByteArray transact(quint8 function, const QByteArray& data, int slaveId, QString* err);

    /** 响应期望总长(含从站+功能码): 按功能码推断; -1=未知 */
    static int expectedRespLen(quint8 function, quint8 byteCount);

    SerialParams m_params;
    QIODevice* m_device = nullptr;      // 默认指向 m_serial
    QSerialPort m_serial;
    bool m_ownsSerial = true;
    QMutex m_mutex;                     // 同一连接可能被不同线程的工具使用

    static QMutex s_poolMutex;
    static QMap<QString, ModbusRtuMaster*> s_pool;
};

} // namespace VisionInspector
