/** @file FitLine.h - 拟合直线工具 (对标 CKVision 拟合线, P1: 从任意点集拟合) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

class FitLine : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("FitLine"); }
    QString displayName() const override { return QStringLiteral("拟合直线"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    QString description() const override { return QStringLiteral(
        "对上游点集(如扫描边缘)做鲁棒直线拟合, 输出中心/角度/rms；用于从点集建线"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;

private:
    bool m_lastOk = false;
    double m_x = 0, m_y = 0, m_angle = 0;
    double m_x1 = 0, m_y1 = 0, m_x2 = 0, m_y2 = 0;
};
} // namespace VisionInspector
