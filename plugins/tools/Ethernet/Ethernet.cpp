#include "Ethernet.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QTcpSocket>
#include <QTcpServer>
#include <QHostAddress>

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
                const QString err = socket->errorString();   // 先拷贝, 再释放 (H-20)
                delete socket;
                setResultData("error", QString("连接失败: %1").arg(err));
                setStatus(ToolStatus::NG);
                return false;
            }
        } else if (action == 1) {
            // 断开
            if (socket) {
                socket->disconnectFromHost();
                socket->waitForDisconnected(1000);
                delete socket;                              // 释放, 防重连泄漏 (H-20)
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
        // 服务器模式: TCP 监听 + 收发(与客户端模式对称)
        void* srvPtr = context.getData("tcpServer").value<void*>();
        QTcpServer* server = static_cast<QTcpServer*>(srvPtr);

        if (action == 0) {
            // 开始监听
            if (server && server->isListening()) {
                setResultData("status", "已在监听");
                setStatus(ToolStatus::OK);
                return true;
            }
            server = new QTcpServer();
            const QString ip = propertyValue("ipAddress").toString();
            const int port = propertyValue("port").toInt();
            const QHostAddress addr =
                (ip.isEmpty() || ip == "0.0.0.0") ? QHostAddress::Any : QHostAddress(ip);
            if (server->listen(addr, static_cast<quint16>(port))) {
                context.setData("tcpServer", QVariant::fromValue((void*)server));
                setResultData("status", "监听中");
                setResultData("listenPort", port);
                setStatus(ToolStatus::OK);
                return true;
            }
            setResultData("error", QString("监听失败: %1").arg(server->errorString()));
            delete server;
            setStatus(ToolStatus::NG);
            return false;
        }

        if (action == 1) {
            // 停止监听
            if (server) {
                server->close();
                delete server;
                context.setData("tcpServer", QVariant());
                setResultData("status", "已停止监听");
            } else {
                setResultData("status", "未在监听");
            }
            setStatus(ToolStatus::OK);
            return true;
        }

        if (action == 2 || action == 3) {
            // 发送 / 接收: 需先接入一个客户端
            if (!server || !server->isListening()) {
                setResultData("error", "服务器未监听");
                setStatus(ToolStatus::NG);
                return false;
            }
            if (!server->hasPendingConnections() && !server->waitForNewConnection(3000)) {
                setResultData("error", "无客户端连接");
                setStatus(ToolStatus::NG);
                return false;
            }
            QTcpSocket* client = server->nextPendingConnection();
            if (action == 3) {
                // 接收
                if (client->waitForReadyRead(3000)) {
                    const QByteArray data = client->readAll();
                    context.setData("receivedData", QString::fromUtf8(data));
                    setResultData("status", "已接收");
                    setResultData("receivedData", QString::fromUtf8(data));
                    setResultData("bytesReceived", data.size());
                    setStatus(ToolStatus::OK);
                    delete client;
                    return true;
                }
                setResultData("error", "接收超时");
                delete client;
                setStatus(ToolStatus::NG);
                return false;
            }
            // 发送
            const QString data = propertyValue("sendData").toString();
            client->write(data.toUtf8());
            client->waitForBytesWritten(3000);
            setResultData("status", "已发送");
            setResultData("bytesSent", data.toUtf8().size());
            setStatus(ToolStatus::OK);
            delete client;
            return true;
        }
    }
    return false;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(EthernetTool, "以太网", VisionInspector::ToolCategory::Communication)
