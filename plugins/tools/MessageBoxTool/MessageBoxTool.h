/** @file MessageBoxTool.h - 提示对话框工具 (对标 CKVision 提示对话框, P0-9) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

/** 提示对话框: 流程执行中弹出提示 (操作工提醒), 支持自动关闭超时 */
class MessageBoxTool : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("MessageBoxTool"); }
    QString displayName() const override { return QStringLiteral("提示对话框"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral(
        "流程执行中弹出提示框提醒操作工, 支持自动关闭超时；用于换型提醒异常告知"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
