/** @file TcpData.h - TCP 数据收发工具 (对标 CKVision 接收/发送数据, P0-10 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class TcpData : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("TcpData"); }
    QString displayName() const override { return QStringLiteral("TCP收发"); }
    ToolCategory category() const override { return ToolCategory::Communication; }
    QString description() const override { return QStringLiteral("单次TCP连接收发数据(NG上报/握手)"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
