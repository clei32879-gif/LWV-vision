/**
 * @file ModbusIoDriver.h
 * @brief Modbus IO 驱动 — 经 PLC/远程IO 的 Modbus 通道实现 IIODriver
 *
 * 替代方案说明 (对应自产 PCIE-N1616 IO 卡的替代路线):
 *   - 原方案: N1616 卡 16DO → PLC X7~X26 硬接线 (视觉结果送 PLC 输入)
 *   - 本驱动: 通过 PLC(或远程IO模块) 的 Modbus 通道直接读写线圈/寄存器,
 *            免去板卡驱动 (无 CC1616.dll 依赖), 软件侧即可完成 IO 输出/输入。
 *   - 适用: 信捷 XD5E (Modbus TCP), 或配 ICP DAS ET-7042/I-8057 等远程IO
 *           (见 docs/调研/N1616替代选型/ 选型报告)。
 *
 * params 字符串约定 (connect 参数):
 *   transport=tcp|rtu
 *   host / port            (tcp)
 *   serialPort / baudRate  (rtu)
 *   slaveId
 *   outBase=0  输出起始地址 (线圈或寄存器)
 *   inBase=0   输入起始地址 (线圈或离散输入)
 *   useCoil=1  输出走线圈(0x), 0=走保持寄存器
 *   例: "transport=tcp;host=192.168.1.10;slaveId=1;outBase=106;useCoil=1"
 *        (写 PLC 线圈 M106 起 = OK阀等, 见筛选机契约)
 */
#pragma once

#include "IIODriver.h"
#include "ModbusTcpMaster.h"
#include "ModbusRtuMaster.h"
#include <QString>
#include <QVector>

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

    int inputCount() const override { return m_inputCount; }
    int outputCount() const override { return m_outputCount; }

private:
    bool parseParams(const QString& params, QString* err);

    QString m_host = "127.0.0.1";
    quint16 m_port = 502;
    QString m_serialPort = "COM1";
    int m_baudRate = 9600;
    int m_slaveId = 1;
    bool m_useTcp = true;
    bool m_useCoil = true;
    int m_outBase = 0;
    int m_inBase = 0;
    int m_inputCount = 16;
    int m_outputCount = 16;

    ModbusTcpMaster* m_tcp = nullptr;
    ModbusRtuMaster* m_rtu = nullptr;
    bool m_connected = false;
};

} // namespace VisionInspector
