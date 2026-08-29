/** @file DataDisplay.h - 数据显示工具（图像叠加文字） */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class DataDisplay : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("DataDisplay"); }
    QString displayName() const override { return QStringLiteral("数据显示"); }
    ToolCategory category() const override { return ToolCategory::Special; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
