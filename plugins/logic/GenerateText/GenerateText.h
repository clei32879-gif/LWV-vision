/** @file GenerateText.h - 生成文本工具 (对标 CKVision 生成文本) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class GenerateText : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("GenerateText"); }
    QString displayName() const override { return QStringLiteral("生成文本"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
