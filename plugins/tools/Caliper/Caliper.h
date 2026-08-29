/** @file Caliper.h - 卡尺测量工具（增强版） */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class Caliper : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("Caliper"); }
    QString displayName() const override { return QStringLiteral("卡尺测量"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
