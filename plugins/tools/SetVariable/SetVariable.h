/** @file SetVariable.h - 设置变量工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class SetVariable : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("SetVariable"); }
    QString displayName() const override { return QStringLiteral("设置变量"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
