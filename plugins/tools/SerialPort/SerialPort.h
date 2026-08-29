/** @file SerialPort.h - 串行口工具 */
#pragma once
#include "../../../src/engine/ITool.h"
#include <QSerialPort>
#include <QSerialPortInfo>
namespace VisionInspector {
class SerialPortTool : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("SerialPort"); }
    QString displayName() const override { return QStringLiteral("串行口"); }
    ToolCategory category() const override { return ToolCategory::Communication; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
