/** @file MultiContourMatch.h - 多轮廓匹配工具 */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class MultiContourMatch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("MultiContourMatch"); }
    QString displayName() const override { return QStringLiteral("多轮廓匹配"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
