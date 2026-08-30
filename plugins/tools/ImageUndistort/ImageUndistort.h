/** @file ImageUndistort.h - 图像去畸变工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ImageUndistort : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ImageUndistort"); }
    QString displayName() const override { return QStringLiteral("图像去畸变"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
