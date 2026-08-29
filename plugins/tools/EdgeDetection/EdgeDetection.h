/** @file EdgeDetection.h - 边缘检测工具（增强版） */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
#include "../../../src/engine/DataJudgment.h"
namespace VisionInspector {
class EdgeDetection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("EdgeDetection"); }
    QString displayName() const override { return QStringLiteral("边缘检测"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
    DataJudgmentManager m_judgment;
};
} // namespace VisionInspector
