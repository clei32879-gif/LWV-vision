/** @file TemplateMatch.h - 模板匹配工具 (灰度匹配: 角度/缩放/多目标, #9) */
#pragma once
#include "../../../src/engine/ITool.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <vector>
#endif
namespace VisionInspector {
class TemplateMatch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("TemplateMatch"); }
    QString displayName() const override { return QStringLiteral("模板匹配"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;
#ifdef VI_HAS_OPENCV
private:
    struct Match { cv::Point2d center; double score; double angle; double scale; };
    std::vector<Match> m_matches;   // 结果缓存(叠加层)
#endif
};
} // namespace VisionInspector
