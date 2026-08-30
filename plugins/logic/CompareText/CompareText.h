/** @file CompareText.h - 比较文本工具 (对标 CKVision 比较文本) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class CompareText : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("CompareText"); }
    QString displayName() const override { return QStringLiteral("比较文本"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
