/** @file PlaySound.h - 播放声音工具 (对标 CKVision 播放声音, P0-10 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class PlaySound : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("PlaySound"); }
    QString displayName() const override { return QStringLiteral("播放声音"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral("NG报警/提示音, 播放wav文件或系统提示音"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
