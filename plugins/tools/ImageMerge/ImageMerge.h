/** @file ImageMerge.h - 图像合并工具 (对标 CKVision 图像合并, P1-11 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ImageMerge : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ImageMerge"); }
    QString displayName() const override { return QStringLiteral("图像合并"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral("多张图像按行列合并成一张大图(带重叠)"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
