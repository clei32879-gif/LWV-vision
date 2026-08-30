/** @file WriteText.h - 写入文本工具 (对标 CKVision 写入文本, P0-10 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class WriteText : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("WriteText"); }
    QString displayName() const override { return QStringLiteral("写入文本"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral("将工具结果/文本保存到指定文件夹文本文件"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
