/** @file ModbusRead.h - Modbus读数据工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ModbusRead : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ModbusRead"); }
    QString displayName() const override { return QStringLiteral("MB读数据"); }
    ToolCategory category() const override { return ToolCategory::Communication; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
