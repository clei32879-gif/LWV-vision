/** @file ModbusComm.h - Modbus通讯工具 (流程工具) */
#pragma once
#include "../../../src/engine/ITool.h"

namespace VisionInspector {

/**
 * ModbusComm - 检测流程中的Modbus通讯工具
 *
 * 在视觉检测流程中读写PLC数据:
 * - 支持寄存器类型: 保持寄存器(HR)、线圈(Coil)、输入寄存器(IR)、离散输入(DI)
 * - 支持读写方向: 读、写
 * - 支持数据类型: INT16、INT32、FLOAT、BOOL
 * - 从ToolContext获取PLC驱动实例进行通讯
 *
 * 典型用法:
 *   1. 读取D1000寄存器值 → 存到上下文供后续工具使用
 *   2. 写入M100线圈触发吹气动作
 *   3. 读取M0判断物料到位
 */
class ModbusComm : public ITool {
    Q_OBJECT

public:
    explicit ModbusComm(QObject* parent = nullptr);
    ~ModbusComm() override = default;

    // ---- 基础信息 ----
    QString typeName() const override;
    QString displayName() const override;
    ToolCategory category() const override;
    QString description() const override;

    // ---- 属性定义 ----
    PropertyDefList propertyDefs() const override;

    // ---- 执行 ----
    bool execute(ToolContext& context) override;

private:
    // 从上下文获取PLC驱动
    IPLCDriver* getPLCDriver(ToolContext& context);

    // 执行读操作
    bool executeRead(IPLCDriver* plc, ToolContext& context);

    // 执行写操作
    bool executeWrite(IPLCDriver* plc, ToolContext& context);
};

} // namespace VisionInspector
