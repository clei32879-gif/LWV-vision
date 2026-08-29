/** @file ImageCorrection.h - 图像补正工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ImageCorrection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ImageCorrection"); }
    QString displayName() const override { return QStringLiteral("图像补正"); }
    ToolCategory category() const override { return ToolCategory::Calibration; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
