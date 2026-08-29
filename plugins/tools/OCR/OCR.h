/** @file OCR.h - 字符识别工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class OCR : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("OCR"); }
    QString displayName() const override { return QStringLiteral("字符识别"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral("字符识别 (预留Tesseract接口)"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
