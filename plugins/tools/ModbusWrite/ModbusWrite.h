/** @file ModbusWrite.h - Modbus写数据工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ModbusWrite : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ModbusWrite"); }
    QString displayName() const override { return QStringLiteral("MB写数据"); }
    ToolCategory category() const override { return ToolCategory::Communication; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
