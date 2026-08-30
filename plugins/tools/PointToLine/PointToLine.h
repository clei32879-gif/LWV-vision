/** @file PointToLine.h - 点到线距离工具 (几何测量) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class PointToLine : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("PointToLine"); }
    QString displayName() const override { return QStringLiteral("点到线距离"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
