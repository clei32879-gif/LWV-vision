#include "Ethernet.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QTcpSocket>
#include <QTcpServer>

namespace VisionInspector {

PropertyDefList EthernetTool::propertyDefs() const {
    return {
        PropertyDef::enumProp("protocol", "传输协议", {"TCP"}, 0),
        PropertyDef::stringProp("ipAddress", "IP地址", "192.168.1.100"),
        PropertyDef::intProp("port", "端口号", 502, 1, 65535),
        PropertyDef::enumProp("role", "角色", {"客户端", "服务器"}, 0),
        PropertyDef::enumProp("action", "操作", {"连接", "断开", "发送", "接收"}, 0),
        PropertyDef::stringProp("sendData", "发送数据", ""),
    };
}

bool EthernetTool::execute(ToolContext& context) {
    int action = propertyValue("action").toInt();
    int role = propertyValue("role").toInt();
    
    if (role == 0) {
        // 客户端模式
        void* ptr = context.getData("tcpSocket").value<void*>();
        QTcpSocket* socket = static_cast<QTcpSocket*>(ptr);
        
        if (action == 0) {
            // 连接
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                setResultData("status", "已连接");
                setStatus(ToolStatus::OK);
                return true;
            }
            
            socket = new QTcpSocket();
            QString ip = propertyValue("ipAddress").toString();
            int port = propertyValue("port").toInt();
            
            socket->connectToHost(ip, port);
            if (socket->waitForConnected(5000)) {
                context.setData("tcpSocket", QVariant::fromValue((void*)socket));
                setResultData("status", "连接成功");
                setResultData("remoteAddress", ip);
                setResultData("remotePort", port);
                setStatus(ToolStatus::OK);
                return true;
            } else {
                delete socket;
                setResultData("error", QString("连接失败: %1").arg(socket->errorString()));
                setStatus(ToolStatus::NG);
                return false;
            }
        } else if (action == 1) {
            // 断开
            if (socket) {
                socket->disconnectFromHost();
                context.setData("tcpSocket", QVariant());
                setResultData("status", "已断开");
                setStatus(ToolStatus::OK);
                return true;
            }
            setResultData("status", "未连接");
            setStatus(ToolStatus::OK);
            return true;
        } else if (action == 2) {
            // 发送
            QString data = propertyValue("sendData").toString();
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                socket->write(data.toUtf8());
                socket->waitForBytesWritten(3000);
                setResultData("status", "已发送");
                setResultData("bytesSent", data.toUtf8().size());
                setStatus(ToolStatus::OK);
                return true;
            }
            setResultData("error", "未连接");
            setStatus(ToolStatus::NG);
            return false;
        } else if (action == 3) {
            // 接收
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                if (socket->waitForReadyRead(3000)) {
                    QByteArray data = socket->readAll();
                    context.setData("receivedData", QString::fromUtf8(data));
                    setResultData("status", "已接收");
                    setResultData("receivedData", QString::fromUtf8(data));
                    setResultData("bytesReceived", data.size());
                    setStatus(ToolStatus::OK);
                    return true;
                }
                setResultData("error", "接收超时");
                setStatus(ToolStatus::NG);
                return false;
            }
            setResultData("error", "未连接");
            setStatus(ToolStatus::NG);
            return false;
        }
    } else {
        // 服务器模式（简化实现）
        setResultData("status", "服务器模式待实现");
        setStatus(ToolStatus::OK);
        return true;
    }
    return false;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(EthernetTool, "以太网", VisionInspector::ToolCategory::Communication)
