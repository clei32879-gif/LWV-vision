/**
 * @file ModbusWrite.cpp
 * @brief Modbus写数据工具 — 通过自研Modbus主站(TCP/RTU)写PLC
 *
 * 典型用途: 检测完成后写OK/NG信号给PLC触发吹气。
 * 写入值支持 "$(工具名.键)" 引用 (值为字符串属性, 引擎自动解析)。
 */
#include "ModbusWrite.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/hal/ModbusTcpMaster.h"
#include "../../../src/hal/ModbusRtuMaster.h"
#include <QRegularExpression>

namespace VisionInspector {

PropertyDefList ModbusWrite::propertyDefs() const {
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
        PropertyDef::enumProp("dataType", "写入类型", {"保持寄存器", "线圈"}, 0, "写入"),
        PropertyDef::intProp("startAddress", "起始地址(0起)", 0, 0, 65535, "写入"),
        PropertyDef::stringProp("writeValue", "写入值", "1", "写入"),
        PropertyDef::stringProp("note", "备注", "", "写入"),
    };
}

bool ModbusWrite::execute(ToolContext& context) {
    const int dataType = propertyValue("dataType").toInt();
    const int startAddr = propertyValue("startAddress").toInt();
    const int slaveId = propertyValue("slaveId").toInt();
    const int commMode = propertyValue("commMode").toInt();
    QString valueStr = propertyValue("writeValue").toString().trimmed();

    // 兜底解析引用 (单工具调试场景引擎已解析过; 这里再兜一次)
    if (valueStr.contains(QStringLiteral("$("))) {
        static const QRegularExpression rx(QStringLiteral("\\$\\(([^)]+)\\)"));
        auto it = rx.globalMatch(valueStr);
        while (it.hasNext()) {
            auto m = it.next();
            const QString ref = m.captured(1);
            const int dot = ref.lastIndexOf('.');
            if (dot <= 0) continue;
            QVariant v = context.getData(ref);
            if (!v.isValid())
                v = context.getData(ref.left(dot) + QLatin1Char('.') + ref.mid(dot + 1));
            if (v.isValid()) valueStr.replace(m.captured(0), v.toString());
        }
    }

    QString err;
    ModbusTcpMaster* tcpMaster = nullptr;
    ModbusRtuMaster* rtuMaster = nullptr;
    if (commMode == 0) {
        const QString host = propertyValue("host").toString();
        const quint16 port = quint16(propertyValue("port").toInt());
        tcpMaster = ModbusTcpMaster::acquire(host, port, &err);
    } else {
        ModbusRtuMaster::SerialParams sp;
        sp.portName = propertyValue("serialPort").toString();
        sp.baudRate = propertyValue("baudRate").toInt();
        sp.dataBits = propertyValue("dataBits").toInt();
        sp.stopBits = propertyValue("stopBits").toInt();
        // M-39修复: parity是枚举索引, 需映射为"无/偶/奇"
        const int parityIdx = propertyValue("parity").toInt();
        sp.parity = QStringList{"无", "偶", "奇"}.value(parityIdx, "无");
        rtuMaster = ModbusRtuMaster::acquire(sp, &err);
    }
    bool ok = false;

    if (dataType == 0) {
        // 保持寄存器: 支持逗号分隔多值 (单值走FC6, 多值走FC16)
        const QStringList parts = valueStr.split(",", Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            setResultData("error", "无写入数据");
            setStatus(ToolStatus::NG);
            return false;
        }
        QVector<quint16> values;
        for (const QString& p : parts) {
            bool numOk = false;
            const int v = p.trimmed().toInt(&numOk);
            if (!numOk) {
                setResultData("error", QString("数值无效: %1").arg(p.trimmed()));
                setStatus(ToolStatus::NG);
                return false;
            }
            values.append(quint16(v));
        }
        ok = values.size() == 1
                 ? (tcpMaster ? tcpMaster->writeRegister(startAddr, values[0], slaveId, &err)
                              : rtuMaster->writeRegister(startAddr, values[0], slaveId, &err))
                 : (tcpMaster ? tcpMaster->writeRegisters(startAddr, values, slaveId, &err)
                              : rtuMaster->writeRegisters(startAddr, values, slaveId, &err));
        if (ok) setResultData("note", propertyValue("note").toString());
        setResultData("written", values[0]);
        setResultData("count", ok ? values.size() : 0);
    } else {
        // 线圈: 值为 0/1/off/on
        const QString low = valueStr.toLower();
        const bool on = (low == "1" || low == "on" || low == "true");
        ok = tcpMaster ? tcpMaster->writeCoil(startAddr, on, slaveId, &err)
                       : rtuMaster->writeCoil(startAddr, on, slaveId, &err);
        if (ok) setResultData("written", on ? 1 : 0);
        setResultData("count", ok ? 1 : 0);
    }

    if (!ok) {
        setResultData("error", err);
        setStatus(ToolStatus::NG);
        return false;
    }

    setResultData("status", QString("写入成功: %1 %2 = %3")
                              .arg(dataType == 0 ? "R" : "C")
                              .arg(startAddr).arg(valueStr));
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ModbusWrite, "MB写数据", VisionInspector::ToolCategory::Communication)
