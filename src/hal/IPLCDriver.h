/**
 * @file IPLCDriver.h
 * @brief PLC驱动抽象接口
 *
 * 用于与PLC通讯, 实现:
 *   - 读写PLC数据 (Modbus)
 *   - 控制吹气信号 (OK/NG)
 *   - 读取伺服位置反馈
 *   - 设定转盘转速
 *   - 接收物料到位信号
 *
 * 实现示例:
 *   class XinjePLCDriver : public IPLCDriver { ... };  // 信捷PLC (Modbus TCP)
 *   class MitsubishiDriver : public IPLCDriver { ... }; // 三菱PLC (备选)
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>

namespace VisionInspector {

/**
 * PLC通讯类型
 */
enum class PLCCommType {
    ModbusTCP,    // Modbus TCP (以太网)
    ModbusRTU,    // Modbus RTU (串口RS485)
    TCP,          // 自定义TCP协议
    Serial,       // 自定义串口协议
};

/**
 * PLC连接参数
 */
struct PLCConnectionParams {
    PLCCommType commType = PLCCommType::ModbusTCP;
    QString ipAddress = "192.168.1.100";  // IP地址
    int port = 502;                       // 端口号 (Modbus默认502)
    QString serialPort = "COM1";          // 串口号 (RTU模式)
    int baudRate = 9600;                  // 波特率
    int slaveId = 1;                      // 从站地址
    int timeoutMs = 1000;                 // 超时
};

/**
 * PLC驱动抽象接口
 */
class IPLCDriver : public QObject {
    Q_OBJECT

public:
    IPLCDriver(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IPLCDriver() = default;

    /**
     * 驱动名称 (如 "信捷XD5", "三菱FX5U")
     */
    virtual QString driverName() const = 0;

    /**
     * 连接PLC
     */
    virtual bool connect(const PLCConnectionParams& params) = 0;

    /**
     * 断开连接
     */
    virtual void disconnect() = 0;

    /**
     * 是否已连接
     */
    virtual bool isConnected() const = 0;

    // --------------------------------------------------------
    // Modbus读写 (地址以PLC内部地址为准)
    // --------------------------------------------------------

    /** 读保持寄存器 (HR, 16位) */
    virtual bool readHoldingRegisters(int address, int count, QVector<quint16>& values) = 0;

    /** 写保持寄存器 (HR, 16位) */
    virtual bool writeHoldingRegisters(int address, const QVector<quint16>& values) = 0;

    /** 读线圈 (0x/1x, 布尔) */
    virtual bool readCoils(int address, int count, QVector<bool>& values) = 0;

    /** 写线圈 (布尔) */
    virtual bool writeCoils(int address, const QVector<bool>& values) = 0;

    /** 读输入寄存器 (IR, 只读) */
    virtual bool readInputRegisters(int address, int count, QVector<quint16>& values) = 0;

    /** 读离散输入 (DI, 只读布尔) */
    virtual bool readDiscreteInputs(int address, int count, QVector<bool>& values) = 0;

    // --------------------------------------------------------
    // 便捷方法 (高级封装)
    // --------------------------------------------------------

    /** 读单个寄存器 */
    quint16 readRegister(int address);

    /** 写单个寄存器 */
    bool writeRegister(int address, quint16 value);

    /** 读单个线圈 */
    bool readBool(int address);

    /** 写单个线圈 */
    bool writeBool(int address, bool value);

    /** 读32位整数(两个寄存器) */
    qint32 readInt32(int address);

    /** 写32位整数 */
    bool writeInt32(int address, qint32 value);

    /** 读32位浮点数 */
    float readFloat(int address);

    /** 写32位浮点数 */
    bool writeFloat(int address, float value);

    // --------------------------------------------------------
    // 业务级接口 (针对视觉筛选设备的特定操作)
    // --------------------------------------------------------

    /**
     * 控制吹气 (OK吹气口)
     * @param durationMs 吹气持续时间(毫秒)
     */
    virtual bool blowOK(int durationMs = 50) = 0;

    /**
     * 控制吹气 (NG吹气口)
     * @param durationMs 吹气持续时间(毫秒)
     */
    virtual bool blowNG(int durationMs = 50) = 0;

    /**
     * 读取伺服当前位置 (脉冲数)
     */
    virtual qint64 readServoPosition() = 0;

    /**
     * 设定转盘转速 (rpm)
     */
    virtual bool setDiskSpeed(int rpm) = 0;

    /**
     * 读取物料到位信号
     */
    virtual bool isMaterialReady() = 0;

signals:
    void connectionChanged(bool connected);
    void errorOccurred(const QString& error);
    void materialReady();  // 物料到位信号
};

} // namespace VisionInspector
