/** @file CircleToCircle.h - 圆到圆距离工具 (几何测量: 两圆心距) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class CircleToCircle : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("CircleToCircle"); }
    QString displayName() const override { return QStringLiteral("圆到圆距离"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
