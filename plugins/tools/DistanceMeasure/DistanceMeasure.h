/** @file DistanceMeasure.h - 距离测量工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class DistanceMeasure : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("DistanceMeasure"); }
    QString displayName() const override { return QStringLiteral("距离测量"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    QString description() const override { return QStringLiteral("测量两点之间的距离"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
