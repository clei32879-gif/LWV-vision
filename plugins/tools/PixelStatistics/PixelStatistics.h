/** @file PixelStatistics.h - 像素统计工具 (对标 CKVision 像素统计, P1-12 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class PixelStatistics : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("PixelStatistics"); }
    QString displayName() const override { return QStringLiteral("像素统计"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral("ROI内灰度均值/标准差/最小/最大 + 阈值像素数与比率"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
