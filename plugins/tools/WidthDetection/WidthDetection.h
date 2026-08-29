/** @file WidthDetection.h - 检测宽窄工具 */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class WidthDetection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("WidthDetection"); }
    QString displayName() const override { return QStringLiteral("检测宽窄"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
