/**
 * @file ModbusRead.cpp
 * @brief Modbus读数据工具 — 通过自研Modbus TCP主站读取PLC数据
 *
 * 结果数据:
 *   value0..valueN : 各寄存器值 (便于后续工具用 "$(工具名.value0)" 引用)
 *   quantity       : 读取数量
 *   status         : 描述
 * 上下文数据:
 *   <resultKey>    : "12,345,6" 十进制逗号串
 */
#include "ModbusRead.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/hal/ModbusTcpMaster.h"
#include "../../../src/hal/ModbusRtuMaster.h"

namespace VisionInspector {

PropertyDefList ModbusRead::propertyDefs() const {
    return {
        PropertyDef::enumProp("commMode", "通讯方式", {"Modbus TCP", "Modbus RTU(串口)"}, 0, "连接"),
        PropertyDef::stringProp("host", "PLC地址", "127.0.0.1", "连接"),
        PropertyDef::intProp("port", "端口", 502, 1, 65535, "连接"),
        PropertyDef::stringProp("serialPort", "串口号", "COM1", "连接"),
        PropertyDef::intProp("baudRate", "波特率", 9600, 0, 100000000, "连接"),
        PropertyDef::intProp("dataBits", "数据位", 8, 5, 8, "连接"),
        PropertyDef::intProp("stopBits", "停止位", 1, 1, 2, "连接"),
        PropertyDef::enumProp("parity", "校验", {"无", "偶", "奇"}, 0, "连接"),
        PropertyDef::intProp("slaveId", "从站地址", 1, 1, 247, "连接"),
        PropertyDef::enumProp("dataType", "数据类型",
            {"保持寄存器(4x)", "输入寄存器(3x)", "线圈(0x)", "离散输入(1x)"}, 0),
        PropertyDef::intProp("startAddress", "起始地址(0起)", 0, 0, 65535, "读取"),
        PropertyDef::intProp("quantity", "读取数量", 1, 1, 125, "读取"),
        PropertyDef::stringProp("resultKey", "结果键名", "modbusData", "读取"),
    };
}

bool ModbusRead::execute(ToolContext& context) {
    const int dataType = propertyValue("dataType").toInt();
    const int startAddr = propertyValue("startAddress").toInt();
    const int quantity = propertyValue("quantity").toInt();
    const int slaveId = propertyValue("slaveId").toInt();
    const QString resultKey = propertyValue("resultKey").toString();
    const int commMode = propertyValue("commMode").toInt();

    QString err;
    QVector<quint16> regs;
    QVector<bool> bits;
    bool isBits = (dataType >= 2);

    if (commMode == 0) {
        // Modbus TCP
        const QString host = propertyValue("host").toString();
        const quint16 port = quint16(propertyValue("port").toInt());
        auto* master = ModbusTcpMaster::acquire(host, port, &err);
        if (isBits) {
            bits = master->readBits(dataType == 2 ? 1 : 2, startAddr, quantity, slaveId, &err);
        } else {
            regs = master->readRegisters(dataType == 0 ? 3 : 4, startAddr, quantity, slaveId, &err);
        }
    } else {
        // Modbus RTU (串口)
        ModbusRtuMaster::SerialParams sp;
        sp.portName = propertyValue("serialPort").toString();
        sp.baudRate = propertyValue("baudRate").toInt();
        sp.dataBits = propertyValue("dataBits").toInt();
        sp.stopBits = propertyValue("stopBits").toInt();
        sp.parity = propertyValue("parity").toString();
        auto* master = ModbusRtuMaster::acquire(sp, &err);
        if (isBits) {
            bits = master->readBits(dataType == 2 ? 1 : 2, startAddr, quantity, slaveId, &err);
        } else {
            regs = master->readRegisters(dataType == 0 ? 3 : 4, startAddr, quantity, slaveId, &err);
        }
    }

    QStringList valuesText;
    bool ok = false;

    if (isBits) {
        ok = !bits.isEmpty();
        for (int i = 0; i < bits.size(); ++i) {
            valuesText << (bits[i] ? "1" : "0");
            setResultData(QString("value%1").arg(i), bits[i] ? 1 : 0);
        }
        if (ok) setResultData("value0", bits[0] ? 1 : 0);
        setResultData("quantity", ok ? bits.size() : 0);
    } else {
        ok = !regs.isEmpty();
        for (int i = 0; i < regs.size(); ++i) {
            valuesText << QString::number(regs[i]);
            setResultData(QString("value%1").arg(i), regs[i]);
        }
        if (ok) setResultData("value0", regs[0]);
        setResultData("quantity", ok ? regs.size() : 0);
    }

    if (!ok) {
        setResultData("error", err);
        setStatus(ToolStatus::NG);
        return false;
    }

    context.setData(resultKey, valuesText.join(","));
    setResultData("status", QString("读取成功: 地址%1 起 %2 个")
                              .arg(startAddr).arg(valuesText.size()));
    setResultData("resultKey", resultKey);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ModbusRead, "MB读数据", VisionInspector::ToolCategory::Communication)
