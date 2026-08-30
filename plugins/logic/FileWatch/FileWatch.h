/** @file FileWatch.h - 文件监测工具 (对标 CKVision 文件监测, P0-10 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class FileWatch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("FileWatch"); }
    QString displayName() const override { return QStringLiteral("文件监测"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral("监测信号文件出现/删除 (现场低成本握手对接)"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
