/** @file ColorDetection.h - 颜色识别工具 */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class ColorDetection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ColorDetection"); }
    QString displayName() const override { return QStringLiteral("颜色识别"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
