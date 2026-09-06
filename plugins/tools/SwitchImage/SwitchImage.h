/** @file SwitchImage.h - 切换图像工具 (对标 CKVision 切换图像, P1) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

/** 切换图像: 按条件从多个命名图像槽中选一张写为当前图像 */
class SwitchImage : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("SwitchImage"); }
    QString displayName() const override { return QStringLiteral("切换图像"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral(
        "按条件在多个命名图像间切换并写为当前图像；用于多路图像源选择"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
