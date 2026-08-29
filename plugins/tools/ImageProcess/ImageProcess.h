/** @file ImageProcess.h - 图像处理工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ImageProcess : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ImageProcess"); }
    QString displayName() const override { return QStringLiteral("图像处理"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
