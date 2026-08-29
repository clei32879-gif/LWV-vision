/** @file Morphology.h - 形态学操作工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class Morphology : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("Morphology"); }
    QString displayName() const override { return QStringLiteral("形态学操作"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral("腐蚀/膨胀/开运算/闭运算"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
