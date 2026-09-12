/** @file FitCircle.h - 拟合圆工具 (对标 CKVision 拟合圆, P1: 从任意点集拟合) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

/** 拟合圆: 对上游点集(如扫描边缘/轮廓点)做鲁棒圆拟合, 输出圆心/半径/rms */
class FitCircle : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("FitCircle"); }
    QString displayName() const override { return QStringLiteral("拟合圆"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    QString description() const override { return QStringLiteral(
        "对上游点集(如扫描边缘/轮廓点)做鲁棒圆拟合, 输出圆心/半径/rms；用于从点集建圆"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;

private:
    bool m_lastOk = false;
    double m_cx = 0, m_cy = 0, m_r = 0;
};
} // namespace VisionInspector
