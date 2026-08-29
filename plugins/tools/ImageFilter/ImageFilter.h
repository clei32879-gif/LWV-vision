/** @file ImageFilter.h - 图像滤波工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ImageFilter : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ImageFilter"); }
    QString displayName() const override { return QStringLiteral("图像滤波"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral("均值/中值/高斯/双边滤波"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
