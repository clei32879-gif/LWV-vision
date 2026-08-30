/** @file CircleToLine.h - 圆到线距离工具 (几何测量: 圆心到直线) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class CircleToLine : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("CircleToLine"); }
    QString displayName() const override { return QStringLiteral("圆到线距离"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
