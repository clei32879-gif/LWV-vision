/** @file LoopEnd.h - 循环结束工具 (对标 CKVision 循环工具"结束循环") */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class LoopEnd : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("LoopEnd"); }
    QString displayName() const override { return QStringLiteral("循环结束"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
