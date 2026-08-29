/** @file Calibration.h - 标定校准工具（像素→毫米转换） */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class Calibration : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("Calibration"); }
    QString displayName() const override { return QStringLiteral("标定校准"); }
    ToolCategory category() const override { return ToolCategory::Calibration; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
