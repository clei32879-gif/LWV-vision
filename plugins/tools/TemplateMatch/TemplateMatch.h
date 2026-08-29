/** @file TemplateMatch.h - 模板匹配工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class TemplateMatch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("TemplateMatch"); }
    QString displayName() const override { return QStringLiteral("模板匹配"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
