/** @file BrightnessCheck.h - 检测亮度工具（增强版） */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class BrightnessCheck : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("BrightnessCheck"); }
    QString displayName() const override { return QStringLiteral("检测亮度"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
