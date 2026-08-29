/** @file LineDetection.h - 检测直线工具（亚像素版：旋转ROI卡尺组+亚像素边缘+鲁棒拟合） */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#endif
#include <vector>
namespace VisionInspector {
class LineDetection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("LineDetection"); }
    QString displayName() const override { return QStringLiteral("检测直线"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;
private:
    ROIRegion m_roi;
#ifdef VI_HAS_OPENCV
    // 结果缓存(供叠加层绘制)
    cv::Point2d m_p1{0, 0}, m_p2{0, 0};
    std::vector<cv::Point2d> m_lastEdges;
    bool m_lastOk = false;
#endif
};
} // namespace VisionInspector
