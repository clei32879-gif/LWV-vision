/** @file CalculateVariable.h - 计算变量工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class CalculateVariable : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("CalculateVariable"); }
    QString displayName() const override { return QStringLiteral("计算变量"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
