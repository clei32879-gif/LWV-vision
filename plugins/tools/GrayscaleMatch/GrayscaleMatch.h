/** @file GrayscaleMatch.h - 灰度匹配定位工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class GrayscaleMatch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("GrayscaleMatch"); }
    QString displayName() const override { return QStringLiteral("灰度匹配"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral("基于灰度的模板匹配，用于定位目标位置"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
