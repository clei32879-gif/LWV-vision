/** @file BarcodeReader.h - 条码识别工具 */
#pragma once
#include "../../../src/engine/ITool.h"
#include <QPointF>
#include <QVector>
namespace VisionInspector {
class BarcodeReader : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("BarcodeReader"); }
    QString displayName() const override { return QStringLiteral("条码识别"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral("识别一维条码 (Code128/Code39/EAN13等)"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;

private:
    bool m_lastFound = false;          // 最近一次是否识别成功 (叠加层用)
    QVector<QPointF> m_lastCorners;    // 首个条码四角 (图像坐标)
};
} // namespace VisionInspector
