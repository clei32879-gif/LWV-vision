/** @file Loop.h - 循环控制工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class Loop : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("循环"); }
    QString displayName() const override { return QStringLiteral("循环"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral("循环执行指定次数"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
