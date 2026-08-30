/** @file SplitText.h - 分解文本工具 (对标 CKVision 分解文本) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class SplitText : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("SplitText"); }
    QString displayName() const override { return QStringLiteral("分解文本"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
