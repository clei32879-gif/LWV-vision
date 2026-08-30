/**
 * @file TcpData.cpp
 * @brief TCP 数据收发工具 (对标 CKVision 接收/发送数据, P0-10 补齐)
 *
 * 单次原子收发: 每次执行自动 连接→(发送)→(接收)→断开, 用完即释放。
 * 适合 NG 结果上报 / 与上位机一次性握手; 无需像"以太网"工具先连接再收发。
 *
 * 操作模式:
 *   发送      连接后发送数据
 *   接收      连接后等待接收
 *   发送并接收 连接后发送, 再等待应答
 *
 * 输出: received / bytesSent / bytesReceived / connected / error
 */
#include "TcpData.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QTcpSocket>
#include <QHostAddress>

namespace VisionInspector {

PropertyDefList TcpData::propertyDefs() const {
    return {
        PropertyDef::stringProp("host", "IP地址", "127.0.0.1"),
        PropertyDef::intProp("port", "端口号", 9000, 1, 65535),
        PropertyDef::intProp("timeoutMs", "超时(ms)", 3000, 100, 60000),
        PropertyDef::enumProp("operation", "操作", {"发送", "接收", "发送并接收"}, 0),
        PropertyDef::stringProp("sendData", "发送数据", ""),
    };
}

bool TcpData::execute(ToolContext& context) {
    const QString host = propertyValue("host").toString();
    const int port = propertyValue("port").toInt();
    const int timeout = propertyValue("timeoutMs").toInt();
    const int operation = propertyValue("operation").toInt();
    const QByteArray sendBytes = propertyValue("sendData").toString().toUtf8();

    QTcpSocket socket;
    socket.connectToHost(host, static_cast<quint16>(port));
    if (!socket.waitForConnected(timeout)) {
        setResultData("connected", false);
        setResultData("error", QStringLiteral("连接失败: %1").arg(socket.errorString()));
        setStatus(ToolStatus::NG);
        return true;
    }
    setResultData("connected", true);
    setResultData("remoteHost", host);
    setResultData("remotePort", port);

    const bool doSend = (operation == 0 || operation == 2);
    const bool doRecv = (operation == 1 || operation == 2);

    int sentBytes = 0;
    if (doSend) {
        const qint64 written = socket.write(sendBytes);
        if (written > 0 && socket.waitForBytesWritten(timeout))
            sentBytes = static_cast<int>(written);
    }
    setResultData("bytesSent", sentBytes);

    QByteArray recv;
    if (doRecv) {
        if (socket.waitForReadyRead(timeout)) {
            recv = socket.readAll();
            // 若数据分片到达, 继续读到超时/无更多
            while (socket.bytesAvailable() > 0 || socket.waitForReadyRead(100)) {
                recv += socket.readAll();
            }
        }
    }
    setResultData("received", QString::fromUtf8(recv));
    setResultData("bytesReceived", static_cast<int>(recv.size()));

    const bool ok = (doSend ? sentBytes > 0 : true) &&
                    (doRecv ? !recv.isEmpty() : true);
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(TcpData, "TCP收发", VisionInspector::ToolCategory::Communication)
