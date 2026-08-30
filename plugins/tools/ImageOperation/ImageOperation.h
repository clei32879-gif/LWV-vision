/** @file ImageOperation.h - 图像运算工具 (对标 CKVision 图像运算, P1-11 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class ImageOperation : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ImageOperation"); }
    QString displayName() const override { return QStringLiteral("图像运算"); }
    ToolCategory category() const override { return ToolCategory::ImageProcess; }
    QString description() const override { return QStringLiteral("两张图像加/减/差分/与/或/异或/最小/最大/平均"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
