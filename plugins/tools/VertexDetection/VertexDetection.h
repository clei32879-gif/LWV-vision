/** @file VertexDetection.h - 检测顶点工具 */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class VertexDetection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("VertexDetection"); }
    QString displayName() const override { return QStringLiteral("检测顶点"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
