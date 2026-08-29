/** @file CaptureImage.h - 采集图像工具（支持相机/文件/列表） */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class CaptureImage : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("CaptureImage"); }
    QString displayName() const override { return QStringLiteral("采集图像"); }
    ToolCategory category() const override { return ToolCategory::Camera; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
