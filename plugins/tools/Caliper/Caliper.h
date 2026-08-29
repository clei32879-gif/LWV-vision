/** @file Caliper.h - 卡尺测量工具（亚像素版：旋转卡尺+梯度峰值抛物线内插） */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#endif
namespace VisionInspector {
class Caliper : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("Caliper"); }
    QString displayName() const override { return QStringLiteral("卡尺测量"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;
private:
    ROIRegion m_roi;
#ifdef VI_HAS_OPENCV
    // 结果缓存(供叠加层绘制)
    cv::Point2d m_scanStart{0, 0}, m_scanEnd{0, 0}, m_edge{0, 0};
    bool m_lastOk = false;
#endif
};
} // namespace VisionInspector
