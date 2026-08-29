/** @file BlobAnalysis.h - 斑点分析工具（增强版） */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class BlobAnalysis : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("BlobAnalysis"); }
    QString displayName() const override { return QStringLiteral("斑点分析"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
