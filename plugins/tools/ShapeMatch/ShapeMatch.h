/** @file ShapeMatch.h - 形状匹配定位工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ShapeMatch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ShapeMatch"); }
    QString displayName() const override { return QStringLiteral("形状匹配"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral("基于轮廓形状的匹配，对旋转和缩放有良好鲁棒性"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
