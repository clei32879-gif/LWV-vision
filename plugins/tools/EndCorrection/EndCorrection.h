/** @file EndCorrection.h - 结束补正工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class EndCorrection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("EndCorrection"); }
    QString displayName() const override { return QStringLiteral("结束补正"); }
    ToolCategory category() const override { return ToolCategory::Calibration; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
