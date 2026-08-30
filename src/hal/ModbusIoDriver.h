/**
 * @file ModbusIoDriver.h
 * @brief Modbus IO 驱动 — 经 PLC/远程IO 实现 IIODriver 接口
 *
 * 用途: 替代独立IO卡(如自产 N1616 / ADAM-4055 等远程IO), 直接用 Modbus
 * 读写 PLC(或远程IO模块)的线圈/寄存器来完成:
 *   - 输出: 相机检测 OK/NG 结果 → PLC 线圈 或 结果寄存器区(如筛选机 D4600 系列)
 *   - 输入: 传感器/物料到位/伺服报警等 → PLC 线圈或离散输入
 *
 * 配置由 params 串传入, 形如:
 *   "transport=tcp;host=192.168.1.10;port=502;slaveId=1"
 *   "transport=rtu;port=COM3;baud=9600;parity=无;slaveId=1"
 *   + 可选 outBase / inBase / 使用线圈或寄存器模式
 *
 * 通道映射 (与 8工位转盘筛选机 契约对齐):
 *   - 输出通道 0..7  → 相机1..8 结果 (写入结果寄存器区 outBase+D)
 *   - 输出通道 8    → OK吹气 (M106)
 *   - 输出通道 9    → NG吹气 (M107)
 *   - 输入通道 0..N → PLC 线圈/离散输入
 * 详细通道含义由 connect() 参数决定 (默认按筛选机契约)。
 */
#pragma once

#include "IIODriver.h"
#include "ModbusTcpMaster.h"
#include "ModbusRtuMaster.h"
#include <QMutex>

namespace VisionInspector {

class ModbusIoDriver : public IIODriver {
    Q_OBJECT

public:
    explicit ModbusIoDriver(QObject* parent = nullptr);
    ~ModbusIoDriver() override;

    QString driverName() const override { return QStringLiteral("ModbusIO"); }

    bool connect(const QString& params) override;
    void disconnect() override;
    bool isConnected() const override;

    bool readInput(int channel, bool& value) override;
    bool readAllInputs(QVector<bool>& values) override;

    bool writeOutput(int channel, bool value) override;
    bool writeAllOutputs(const QVector<bool>& values) override;

    bool pulseOutput(int channel, int durationMs) override;

    int inputCount() const override { return m_p.inputCount; }
    int outputCount() const override { return m_p.outputCount; }

private:
    struct Params {
        bool useTcp = true;
        QString host = "127.0.0.1";
        quint16 port = 502;
        QString serialPort = "COM1";
        int baudRate = 9600;
        int slaveId = 1;
        // 结果写入模式: coilOut=true 写线圈 (M), false 写保持寄存器 (结果数组)
        bool coilOut = false;
        int outBase = 0;      // 输出起始地址 (线圈M号 或 寄存器偏移)
        int inBase = 0;       // 输入起始地址 (线圈M号 或 离散输入)
        bool readInputAsCoil = true;   // true=读线圈, false=读离散输入(FC2)
        int timeoutMs = 500;
        int inputCount = 16;
        int outputCount = 16;
    };
    bool parseParams(const QString& params, QString* err);
    void releaseMaster();
    void disconnectInternal();   // 不加锁的内部断开

    Params m_p;
    ModbusTcpMaster* m_tcp = nullptr;
    ModbusRtuMaster* m_rtu = nullptr;
    bool m_connected = false;
    QMutex m_mutex;
};

} // namespace VisionInspector
