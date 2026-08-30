/** @file EdgeSpacing.h - 检测间距工具 (对标 CKVision 检测间距, P1-12 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class EdgeSpacing : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("EdgeSpacing"); }
    QString displayName() const override { return QStringLiteral("检测间距"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    QString description() const override { return QStringLiteral("沿扫描线检测边缘, 计算相邻边缘间距(位置/宽度)"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
