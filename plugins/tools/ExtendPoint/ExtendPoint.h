/** @file ExtendPoint.h - 延伸点工具 (对标 CKVision 延伸点) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ExtendPoint : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ExtendPoint"); }
    QString displayName() const override { return QStringLiteral("延伸点"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
