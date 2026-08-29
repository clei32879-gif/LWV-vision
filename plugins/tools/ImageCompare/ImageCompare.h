/** @file ImageCompare.h - 图像对比工具（来自CKVisionBuilder缺陷检测） */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ImageCompare : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ImageCompare"); }
    QString displayName() const override { return QStringLiteral("图像对比"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
