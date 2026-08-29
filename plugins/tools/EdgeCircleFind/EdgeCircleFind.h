/** @file EdgeCircleFind.h - 快速找圆工具 (EdgeDrawing整图搜索, 用于定位/引导) */
#pragma once
#include "../../../src/engine/ITool.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#endif
namespace VisionInspector {
class EdgeCircleFind : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("EdgeCircleFind"); }
    QString displayName() const override { return QStringLiteral("快速找圆"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;
private:
#ifdef VI_HAS_OPENCV
    cv::Point2d m_lastCenter{0, 0};
    double m_lastRadius = 0;
    bool m_lastOk = false;
#endif
};
} // namespace VisionInspector
