/** @file SerialPort.h - SerialPort通讯工具 */
#pragma once
#include "../../../src/engine/ITool.h"
class QSerialPort;
namespace VisionInspector {
class SerialPort : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("SerialPort"); }
    QString displayName() const override { return QStringLiteral("串口通讯"); }
    QString description() const override { return QStringLiteral("串口数据发送与接收"); }
    ToolCategory category() const override { return ToolCategory::Communication; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
