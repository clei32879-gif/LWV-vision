/**
 * @file ModbusComm.cpp
 * @brief Modbus通讯流程工具实现
 *
 * 作为ITool在检测流程中执行PLC数据读写操作。
 * 通过ToolContext获取PLC驱动实例，与PLC进行Modbus通讯。
 *
 * 属性说明:
 *   registerType: 寄存器类型 (保持寄存器/线圈/输入寄存器/离散输入)
 *   direction:    读写方向 (读/写)
 *   dataType:     数据类型 (INT16/INT32/FLOAT/BOOL)
 *   address:      寄存器起始地址
 *   count:        读取/写入数量
 *   value:        写入值 (写模式使用, 支持逗号分隔多值)
 *   resultKey:    结果键名 (读模式, 存储到上下文)
 *   slaveId:      从站地址 (可选, 覆盖默认设置)
 */

#include "ModbusComm.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/hal/IPLCDriver.h"
#include "../../../src/core/PluginManager.h"
#include <QDebug>
#include <cstring>

namespace VisionInspector {

// ============================================================
// 构造
// ============================================================

ModbusComm::ModbusComm(QObject* parent)
    : ITool(parent)
{
}

// ============================================================
// 基础信息
// ============================================================

QString ModbusComm::typeName() const
{
    return QStringLiteral("ModbusComm");
}

QString ModbusComm::displayName() const
{
    return QStringLiteral("Modbus通讯");
}

ToolCategory ModbusComm::category() const
{
    return ToolCategory::Communication;
}

QString ModbusComm::description() const
{
    return QStringLiteral("在检测流程中读写PLC寄存器和线圈数据，"
                          "支持保持寄存器、线圈、输入寄存器、离散输入四种类型，"
                          "支持INT16/INT32/FLOAT/BOOL数据类型。");
}

// ============================================================
// 属性定义
// ============================================================

PropertyDefList ModbusComm::propertyDefs() const
{
    return {
        // 寄存器类型
        PropertyDef::enumProp("registerType", "寄存器类型",
            {"保持寄存器(HR)", "线圈(Coil)", "输入寄存器(IR)", "离散输入(DI)"}, 0),

        // 读写方向
        PropertyDef::enumProp("direction", "读写方向",
            {"读取", "写入"}, 0),

        // 数据类型
        PropertyDef::enumProp("dataType", "数据类型",
            {"INT16", "INT32", "FLOAT", "BOOL"}, 0),

        // PLC地址
        PropertyDef::intProp("address", "起始地址", 0, 0, 65535),

        // 数量
        PropertyDef::intProp("count", "数据数量", 1, 1, 125),

        // 写入值 (写模式)
        PropertyDef::stringProp("value", "写入值", "0", "写入"),

        // 结果键名 (读模式)
        PropertyDef::stringProp("resultKey", "结果键名", "modbusCommData", "读取"),

        // 从站地址 (可选覆盖)
        PropertyDef::intProp("slaveId", "从站地址", 0, 0, 247, "高级"),
    };
}

// ============================================================
// 获取PLC驱动
// ============================================================

IPLCDriver* ModbusComm::getPLCDriver(ToolContext& context)
{
    // 优先从上下文获取
    IPLCDriver* plc = context.plcDriver();
    if (plc && plc->isConnected()) {
        return plc;
    }

    // 备选: 从PluginManager或全局变量获取
    // 此处简化处理 - 实际可能需要通过HardwareManager获取
    qWarning() << "[ModbusComm] 无法从上下文获取PLC驱动或PLC未连接";
    return nullptr;
}

// ============================================================
// 执行
// ============================================================

bool ModbusComm::execute(ToolContext& context)
{
    // 获取PLC驱动
    IPLCDriver* plc = getPLCDriver(context);
    if (!plc) {
        setResultData("error", "PLC驱动未就绪或未连接");
        setStatus(ToolStatus::NG);
        return false;
    }

    // 根据读写方向执行
    int direction = propertyValue("direction").toInt();
    if (direction == 0) {
        return executeRead(plc, context);
    } else {
        return executeWrite(plc, context);
    }
}

// ============================================================
// 读操作
// ============================================================

bool ModbusComm::executeRead(IPLCDriver* plc, ToolContext& context)
{
    int registerType = propertyValue("registerType").toInt();  // 0=HR,1=Coil,2=IR,3=DI
    int dataType     = propertyValue("dataType").toInt();       // 0=INT16,1=INT32,2=FLOAT,3=BOOL
    int address      = propertyValue("address").toInt();
    int count        = propertyValue("count").toInt();
    QString resultKey = propertyValue("resultKey").toString();

    if (resultKey.isEmpty()) {
        resultKey = "modbusCommData";
    }

    bool ok = false;

    switch (registerType) {
    case 0:  // 保持寄存器 (HR)
    {
        QVector<quint16> values;
        // INT32/FLOAT 需要读2个寄存器，BOOL 额外读1个
        int regCount = count;
        if (dataType == 1 || dataType == 2) {
            regCount = count * 2; // INT32/FLOAT每个值占2个寄存器
        }

        ok = plc->readHoldingRegisters(address, regCount, values);
        if (ok) {
            // 根据数据类型转换并存储结果
            QVariantList resultList;
            for (int i = 0; i < count; ++i) {
                switch (dataType) {
                case 0:  // INT16
                    if (i < values.size()) {
                        resultList.append(static_cast<qint16>(values[i]));
                    }
                    break;
                case 1:  // INT32
                    if (i * 2 + 1 < values.size()) {
                        // H-9修复: 用quint32组合避免有符号左移UB + 符号扩展错误
                        const quint32 raw = (static_cast<quint32>(values[i * 2]) << 16)
                                         | static_cast<quint32>(values[i * 2 + 1]);
                        qint32 val32;
                        memcpy(&val32, &raw, sizeof(val32));
                        resultList.append(val32);
                    }
                    break;
                case 2:  // FLOAT
                    if (i * 2 + 1 < values.size()) {
                        quint32 raw = (static_cast<quint32>(values[i * 2]) << 16)
                                    | static_cast<quint32>(values[i * 2 + 1]);
                        float fval;
                        memcpy(&fval, &raw, sizeof(float));
                        resultList.append(fval);
                    }
                    break;
                case 3:  // BOOL
                    if (i < values.size()) {
                        resultList.append(values[i] != 0);
                    }
                    break;
                }
            }

            context.setData(resultKey, resultList.size() == 1 ? resultList[0] : QVariant(resultList));
            setResultData("value", resultList.size() == 1 ? resultList[0] : QVariant(resultList));
        }
        break;
    }
    case 1:  // 线圈 (Coil)
    {
        QVector<bool> values;
        ok = plc->readCoils(address, count, values);
        if (ok && !values.isEmpty()) {
            if (count == 1) {
                context.setData(resultKey, values[0]);
                setResultData("value", values[0]);
            } else {
                QVariantList boolList;
                for (bool v : values) boolList.append(v);
                context.setData(resultKey, QVariant(boolList));
                setResultData("value", QVariant(boolList));
            }
        }
        break;
    }
    case 2:  // 输入寄存器 (IR)
    {
        QVector<quint16> values;
        ok = plc->readInputRegisters(address, count, values);
        if (ok && !values.isEmpty()) {
            if (count == 1) {
                context.setData(resultKey, values[0]);
                setResultData("value", values[0]);
            } else {
                QVariantList regList;
                for (quint16 v : values) regList.append(v);
                context.setData(resultKey, QVariant(regList));
                setResultData("value", QVariant(regList));
            }
        }
        break;
    }
    case 3:  // 离散输入 (DI)
    {
        QVector<bool> values;
        ok = plc->readDiscreteInputs(address, count, values);
        if (ok && !values.isEmpty()) {
            if (count == 1) {
                context.setData(resultKey, values[0]);
                setResultData("value", values[0]);
            } else {
                QVariantList boolList;
                for (bool v : values) boolList.append(v);
                context.setData(resultKey, QVariant(boolList));
                setResultData("value", QVariant(boolList));
            }
        }
        break;
    }
    }

    if (ok) {
        setResultData("status", "读取成功");
        setResultData("address", address);
        setResultData("count", count);
        setResultData("resultKey", resultKey);
        setStatus(ToolStatus::OK);

        qDebug() << "[ModbusComm] 读取成功: 地址=" << address
                 << "数量=" << count << "键名=" << resultKey;
        return true;
    }

    setResultData("error", QString("读取失败: 地址=%1 数量=%2").arg(address).arg(count));
    setStatus(ToolStatus::NG);
    return false;
}

// ============================================================
// 写操作
// ============================================================

bool ModbusComm::executeWrite(IPLCDriver* plc, ToolContext& context)
{
    int registerType = propertyValue("registerType").toInt();  // 0=HR,1=Coil,2=IR,3=DI
    int dataType     = propertyValue("dataType").toInt();       // 0=INT16,1=INT32,2=FLOAT,3=BOOL
    int address      = propertyValue("address").toInt();
    QString valueStr = propertyValue("value").toString();

    if (valueStr.isEmpty()) {
        setResultData("error", "写入值为空");
        setStatus(ToolStatus::NG);
        return false;
    }

    bool ok = false;

    // 解析写入值 (逗号分隔多值)
    QStringList parts = valueStr.split(",", Qt::SkipEmptyParts);

    switch (registerType) {
    case 0:  // 保持寄存器 (HR)
    {
        QVector<quint16> values;

        for (const QString& part : parts) {
            QString trimmed = part.trimmed();

            switch (dataType) {
            case 0:  // INT16
                values.append(static_cast<quint16>(trimmed.toInt()));
                break;
            case 1:  // INT32
            {
                qint32 val32 = static_cast<qint32>(trimmed.toLongLong());
                values.append(static_cast<quint16>((val32 >> 16) & 0xFFFF)); // 高字
                values.append(static_cast<quint16>(val32 & 0xFFFF));        // 低字
                break;
            }
            case 2:  // FLOAT
            {
                float fval = trimmed.toFloat();
                quint32 raw;
                memcpy(&raw, &fval, sizeof(float));
                values.append(static_cast<quint16>((raw >> 16) & 0xFFFF)); // 高字
                values.append(static_cast<quint16>(raw & 0xFFFF));         // 低字
                break;
            }
            case 3:  // BOOL → 作为0/1写入寄存器
                values.append(static_cast<quint16>(trimmed.toInt() ? 1 : 0));
                break;
            }
        }

        ok = plc->writeHoldingRegisters(address, values);
        break;
    }
    case 1:  // 线圈 (Coil)
    {
        QVector<bool> values;
        for (const QString& part : parts) {
            QString trimmed = part.trimmed();
            values.append(trimmed.toInt() != 0 || trimmed.compare("true", Qt::CaseInsensitive) == 0);
        }

        ok = plc->writeCoils(address, values);
        break;
    }
    default:
        // IR和DI是只读的，不支持写入
        setResultData("error", "输入寄存器和离散输入不支持写入操作");
        setStatus(ToolStatus::NG);
        return false;
    }

    if (ok) {
        setResultData("status", "写入成功");
        setResultData("address", address);
        setResultData("value", valueStr);
        setStatus(ToolStatus::OK);

        qDebug() << "[ModbusComm] 写入成功: 地址=" << address
                 << "值=" << valueStr;
        return true;
    }

    setResultData("error", QString("写入失败: 地址=%1 值=%2").arg(address).arg(valueStr));
    setStatus(ToolStatus::NG);
    return false;
}

} // namespace VisionInspector

// ============================================================
// 工具注册
// ============================================================

VI_REGISTER_TOOL(ModbusComm, "Modbus通讯", VisionInspector::ToolCategory::Communication)
