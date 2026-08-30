/** @file DataQueue.h - 数据队列工具 (对标 CKVision 队列, P0-9 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class DataQueue : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("DataQueue"); }
    QString displayName() const override { return QStringLiteral("数据队列"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
