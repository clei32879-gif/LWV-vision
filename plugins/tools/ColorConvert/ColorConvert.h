/** @file ColorConvert.h - 颜色空间转换工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ColorConvert : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ColorConvert"); }
    QString displayName() const override { return QStringLiteral("颜色空间转换"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral("RGB/HSV/GRAY/LAB颜色空间转换"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
