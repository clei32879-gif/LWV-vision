/** @file RotatePoint.h - 旋转点工具 (对标 CKVision 旋转点) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class RotatePoint : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("RotatePoint"); }
    QString displayName() const override { return QStringLiteral("旋转点"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
