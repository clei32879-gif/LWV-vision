/** @file BarcodeReader.h - 条码识别工具 */
#pragma once
#include "../../../src/engine/ITool.h"
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
};
} // namespace VisionInspector
