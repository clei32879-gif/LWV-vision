/** @file ContourCompare.h - 轮廓对比工具 */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class ContourCompare : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ContourCompare"); }
    QString displayName() const override { return QStringLiteral("轮廓对比"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
