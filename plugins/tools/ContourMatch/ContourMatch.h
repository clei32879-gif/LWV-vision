/** @file ContourMatch.h - 轮廓匹配定位工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ContourMatch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ContourMatch"); }
    QString displayName() const override { return QStringLiteral("轮廓匹配"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral("基于边缘轮廓的匹配，适用于高精度定位"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
