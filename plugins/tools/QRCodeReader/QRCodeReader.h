/** @file QRCodeReader.h - 二维码识别工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class QRCodeReader : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("QRCodeReader"); }
    QString displayName() const override { return QStringLiteral("二维码识别"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral("识别QR Code二维码"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
