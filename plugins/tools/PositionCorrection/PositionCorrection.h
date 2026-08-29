/** @file PositionCorrection.h - 位置补正工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class PositionCorrection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("PositionCorrection"); }
    QString displayName() const override { return QStringLiteral("位置补正"); }
    ToolCategory category() const override { return ToolCategory::Calibration; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
