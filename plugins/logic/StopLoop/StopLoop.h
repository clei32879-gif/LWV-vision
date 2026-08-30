/** @file StopLoop.h - 停止循环工具 (对标 CKVision 停止循环, P0-9 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class StopLoop : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("StopLoop"); }
    QString displayName() const override { return QStringLiteral("停止循环"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
