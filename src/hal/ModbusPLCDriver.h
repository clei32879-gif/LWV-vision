/**
 * @file ModbusPLCDriver.h
 * @brief 信捷 XD 系列 PLC 驱动 — 经 Modbus 主站实现 IPLCDriver
 *
 * 覆盖 8工位转盘筛选机 业务契约 (见 docs/完美化路线图.md 阶段5 与
 * 与上位机通讯契约):
 *   - 保持寄存器: D 区 = 41088 + D号 (HD0 手动速度, D6 命令, HD122 良率,
 *     HD170~176 产量统计, HD200~214 相机位置, HD400/402 剔除位置 ...)
 *   - 线圈: M 区直接映射 (M50 伺服正转, M52 反转, M106 OK阀, M107 NG1阀,
 *     M108 NG2阀, M116/117 手动OK/NG)
 *   - 输入: X 区 (离散输入) 与 M 区
 *
 * 同时支持 Modbus TCP (以太网) 与 Modbus RTU (串口 RS232/485)。
 */
#pragma once

#include "IPLCDriver.h"
#include "ModbusTcpMaster.h"
#include "ModbusRtuMaster.h"

namespace VisionInspector {

class ModbusPLCDriver : public IPLCDriver {
    Q_OBJECT

public:
    explicit ModbusPLCDriver(QObject* parent = nullptr);
    ~ModbusPLCDriver() override;

    QString driverName() const override { return QStringLiteral("信捷XD Modbus"); }

    bool connect(const PLCConnectionParams& params) override;
    void disconnect() override;
    bool isConnected() const override;

    // ---- Modbus 核心读写 (地址 = PLC 内部地址) ----
    bool readHoldingRegisters(int address, int count, QVector<quint16>& values) override;
    bool writeHoldingRegisters(int address, const QVector<quint16>& values) override;
    bool readCoils(int address, int count, QVector<bool>& values) override;
    bool writeCoils(int address, const QVector<bool>& values) override;
    bool readInputRegisters(int address, int count, QVector<quint16>& values) override;
    bool readDiscreteInputs(int address, int count, QVector<bool>& values) override;

    // ---- 业务级 (8工位转盘筛选机契约) ----
    bool blowOK(int durationMs = 50) override;      // M106 OK吹气
    bool blowNG(int durationMs = 50) override;      // M107 NG1吹气
    qint64 readServoPosition() override;            // 伺服位置 (读HD220起2字, 契约可配)
    bool setDiskSpeed(int rpm) override;            // 写 HD0 手动速度
    bool isMaterialReady() override;                // 读 X0 物料到位 (离散输入)

    // 契约便捷接口 (供 UI/流程直接使用)
    bool setManualSpeed(int rpm);                   // HD0
    bool setAutoSpeed(int rpm);                     // HD8
    bool sendCommand(int cmd);                      // D6 (0=停 1=启动 3=清零)
    int readOKCount();                              // HD170
    int readNGCount();                              // HD172
    int readTotalCount();                           // HD174
    int readYieldPermille();                        // HD122 (千分比)
    int readUph();                                  // D196
    bool manualShot(int camera);                    // M121+camera-1
    bool manualOK();                                // M116
    bool manualNG();                                // M117

    // 伺服启停
    bool servoForward(bool on);                     // M50
    bool servoReverse(bool on);                     // M52

    int slaveId() const { return m_slaveId; }
    /** 运行时切换从站地址 (ModbusComm 工具的 slaveId 属性覆盖用; 0=恢复默认) */
    void setSlaveId(int id) { if (id > 0) m_slaveId = id; }

private:
    void releaseMaster();

    ModbusTcpMaster* m_tcp = nullptr;
    ModbusRtuMaster* m_rtu = nullptr;
    bool m_useTcp = true;
    int m_slaveId = 1;
    bool m_connected = false;
};

} // namespace VisionInspector
