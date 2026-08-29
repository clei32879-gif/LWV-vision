/** @file UpdateView.h - 更新视图工具（设置显示图像） */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class UpdateView : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("UpdateView"); }
    QString displayName() const override { return QStringLiteral("更新视图"); }
    ToolCategory category() const override { return ToolCategory::Special; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
