/** @file LineToLine.h - 线到线测量工具 (对标 CKVision 线到线) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class LineToLine : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("LineToLine"); }
    QString displayName() const override { return QStringLiteral("线到线"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
