/** @file CropTransform.h - 裁剪变换工具 (对标 CKVision 裁剪变换, P1-11 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class CropTransform : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("CropTransform"); }
    QString displayName() const override { return QStringLiteral("裁剪变换"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral("ROI裁剪 + 镜像/旋转/缩放/平移变换"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
