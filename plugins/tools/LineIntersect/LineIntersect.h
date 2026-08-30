/** @file LineIntersect.h - 两线交点工具 (几何测量) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class LineIntersect : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("LineIntersect"); }
    QString displayName() const override { return QStringLiteral("两线交点"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
