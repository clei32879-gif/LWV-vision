/** @file SaveImage.h - 存储图像工具 (对标 CKVision 存储图像, P1-11 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class SaveImage : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("SaveImage"); }
    QString displayName() const override { return QStringLiteral("存储图像"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral("将当前图像保存到指定目录(可加日期/时间后缀)"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
