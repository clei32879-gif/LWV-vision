/** @file Delay.h - 延时工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class Delay : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("延时"); }
    QString displayName() const override { return QStringLiteral("延时"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral("延时指定毫秒数"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
