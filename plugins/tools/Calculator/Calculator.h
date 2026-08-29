/** @file Calculator.h - 计算器工具（来自CKVisionBuilder） */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class Calculator : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("Calculator"); }
    QString displayName() const override { return QStringLiteral("计算器"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
