/** @file Ethernet.h - 以太网工具 */
#pragma once
#include "../../../src/engine/ITool.h"
#include <QTcpSocket>
#include <QTcpServer>
namespace VisionInspector {
class EthernetTool : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("Ethernet"); }
    QString displayName() const override { return QStringLiteral("以太网"); }
    ToolCategory category() const override { return ToolCategory::Communication; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
