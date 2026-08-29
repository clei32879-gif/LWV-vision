/** @file AngleMeasure.h - 角度测量工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class AngleMeasure : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("AngleMeasure"); }
    QString displayName() const override { return QStringLiteral("角度测量"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    QString description() const override { return QStringLiteral("测量三点构成的角度"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
