/** @file Threshold.h - 阈值分割工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class Threshold : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("Threshold"); }
    QString displayName() const override { return QStringLiteral("阈值分割"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral("固定/OTSU/自适应阈值分割"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
