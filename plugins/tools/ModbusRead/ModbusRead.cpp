#include "ModbusRead.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QTcpSocket>

namespace VisionInspector {

PropertyDefList ModbusRead::propertyDefs() const {
    return {
        PropertyDef::enumProp("dataType", "数据类型", {"保持寄存器", "输入寄存器", "线圈", "离散输入"}, 0),
        PropertyDef::intProp("startAddress", "起始地址", 0, 0, 65535),
        PropertyDef::intProp("quantity", "读取数量", 1, 1, 125),
        PropertyDef::intProp("slaveId", "从站地址", 1, 1, 247),
        PropertyDef::stringProp("resultKey", "结果键名", "modbusData"),
    };
}

bool ModbusRead::execute(ToolContext& context) {
    void* ptr = context.getData("tcpSocket").value<void*>();
    QTcpSocket* socket = static_cast<QTcpSocket*>(ptr);
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        setResultData("error", "未连接到Modbus设备");
        setStatus(ToolStatus::NG);
        return false;
    }
    
    int dataType = propertyValue("dataType").toInt();
    int startAddr = propertyValue("startAddress").toInt();
    int quantity = propertyValue("quantity").toInt();
    int slaveId = propertyValue("slaveId").toInt();
    QString resultKey = propertyValue("resultKey").toString();
    
    // 构建Modbus请求 (简化实现)
    QByteArray request;
    request.append((char)slaveId);
    request.append((char)(dataType + 3)); // 功能码
    request.append((char)((startAddr >> 8) & 0xFF));
    request.append((char)(startAddr & 0xFF));
    request.append((char)((quantity >> 8) & 0xFF));
    request.append((char)(quantity & 0xFF));
    
    // 添加CRC
    quint16 crc = 0xFFFF;
    for (int i = 0; i < request.size(); ++i) {
        crc ^= (quint8)request[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    request.append((char)((crc >> 8) & 0xFF));
    request.append((char)(crc & 0xFF));
    
    // 发送请求
    socket->write(request);
    socket->waitForBytesWritten(1000);
    
    // 等待响应
    if (socket->waitForReadyRead(3000)) {
        QByteArray response = socket->readAll();
        if (response.size() > 3) {
            // 解析响应
            QVector<quint16> values;
            for (int i = 3; i < response.size() - 2; i += 2) {
                quint16 value = ((quint8)response[i] << 8) | (quint8)response[i + 1];
                values.append(value);
            }
            context.setData(resultKey, QVariant::fromValue(values));
            setResultData("status", "读取成功");
            setResultData("quantity", values.size());
            setResultData("resultKey", resultKey);
            setStatus(ToolStatus::OK);
            return true;
        }
    }
    
    setResultData("error", "读取超时或响应错误");
    setStatus(ToolStatus::NG);
    return false;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ModbusRead, "MB读数据", VisionInspector::ToolCategory::Communication)
