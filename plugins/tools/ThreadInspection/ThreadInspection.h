/**
 * @file ThreadInspection.h
 * @brief 螺纹检测专项工具 — 阶段3 拳头功能
 *
 * 管线 (业界标准, 见 docs/开源引入调研-2026-08.md):
 *   定位圆心 → cv::warpPolar 极坐标展开成条带 → 螺纹环带角度投影
 *   → 峰值计数=牙数 / 峰间距=牙距 → 幅值/间距分析判定缺陷
 *
 * 检测项:
 *   - 牙数统计 toothCount
 *   - 牙距 pitchDeg/pitchPx
 *   - 缺牙 missingCount (期望牙数已知时按间距找缺口 / 自动模式按中位间距)
 *   - 烂牙 damageCount (牙型幅值坍塌)
 *   - 毛刺 burrCount (外缘带外亮点)
 *   - 斜牙 slantAngle (内圈/外圈牙型相位差)
 *   - 牙外径 outerDiameterPx
 */
#pragma once
#include "../../../src/engine/ITool.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#endif
#include <vector>

namespace VisionInspector {

class ThreadInspection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ThreadInspection"); }
    QString displayName() const override { return QStringLiteral("螺纹检测"); }
    ToolCategory category() const override { return ToolCategory::Special; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;

private:
#ifdef VI_HAS_OPENCV
    // 叠加绘制缓存
    cv::Point2d m_center{0, 0};
    double m_outerR = 0;
    std::vector<double> m_toothAngles;      // 正常牙角度(度)
    std::vector<double> m_missingAngles;    // 缺口中心角度(度)
    std::vector<cv::Point2d> m_burrPoints;  // 毛刺点
    std::vector<double> m_damageAngles;     // 烂牙角度(度)
    double m_slantAngle = 0;                // 斜牙角(度), 供叠加显示
    bool m_lastOk = false;
#endif
};

} // namespace VisionInspector
