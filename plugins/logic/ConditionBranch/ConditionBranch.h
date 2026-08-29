/** @file ConditionBranch.h - ConditionBranch逻辑工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ConditionBranch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("条件分支"); }
    QString displayName() const override { return QStringLiteral("条件分支"); }
    QString description() const override { return QStringLiteral("根据条件表达式判断流程走向"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
