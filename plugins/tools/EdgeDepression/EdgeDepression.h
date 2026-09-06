/** @file EdgeDepression.h - 边缘凹陷工具 (对标 CKVision 边缘凹陷: 缺件/崩边/缺口检测) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

/**
 * 边缘凹陷: 沿扫描方向提取目标边缘点 → 鲁棒拟合直线基线 → 找偏离基线的凹陷/凸起段
 * 场景: 螺丝缺牙段、工件崩边、料带缺口 (边缘轮廓局部偏离理论直线)
 */
class EdgeDepression : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("EdgeDepression"); }
    QString displayName() const override { return QStringLiteral("边缘凹陷"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral(
        "沿扫描线提取边缘并拟合基线, 检测凹陷/凸起段(深度+宽度)判定; 用于缺件崩边缺口"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;

private:
    bool m_lastOk = false;
    struct Seg { double x1, y1, x2, y2, depth; };
    QVector<Seg> m_lastSegs;         // 凹陷/凸起段 (图像坐标)
    double m_baseX1 = 0, m_baseY1 = 0, m_baseX2 = 0, m_baseY2 = 0;
};
} // namespace VisionInspector
