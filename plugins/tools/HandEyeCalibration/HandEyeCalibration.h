/** @file HandEyeCalibration.h - 手眼标定工具(相机-机器人坐标关系) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class HandEyeCalibration : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("HandEyeCalibration"); }
    QString displayName() const override { return QStringLiteral("手眼标定"); }
    ToolCategory category() const override { return ToolCategory::Calibration; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
