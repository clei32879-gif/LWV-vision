/** @file CoordinateCalibration.h - 坐标校准工具(对标CKVision坐标校准1/2) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class CoordinateCalibration : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("CoordinateCalibration"); }
    QString displayName() const override { return QStringLiteral("坐标校准"); }
    ToolCategory category() const override { return ToolCategory::Calibration; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
