#include "ModbusWrite.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QTcpSocket>

namespace VisionInspector {

PropertyDefList ModbusWrite::propertyDefs() const {
    return {
        PropertyDef::enumProp("dataType", "数据类型", {"保持寄存器", "线圈"}, 0),
        PropertyDef::intProp("startAddress", "起始地址", 0, 0, 65535),
        PropertyDef::intProp("slaveId", "从站地址", 1, 1, 247),
        PropertyDef::stringProp("writeData", "写入数据", "0"),
        PropertyDef::stringProp("dataKey", "数据链接", ""),
    };
}

bool ModbusWrite::execute(ToolContext& context) {
    void* ptr = context.getData("tcpSocket").value<void*>();
    QTcpSocket* socket = static_cast<QTcpSocket*>(ptr);
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        setResultData("error", "未连接到Modbus设备");
        setStatus(ToolStatus::NG);
        return false;
    }
    
    int dataType = propertyValue("dataType").toInt();
    int startAddr = propertyValue("startAddress").toInt();
    int slaveId = propertyValue("slaveId").toInt();
    
    // 获取要写入的数据
    QVector<quint16> values;
    QString dataKey = propertyValue("dataKey").toString();
    if (!dataKey.isEmpty() && context.hasData(dataKey)) {
        values = context.getData(dataKey).value<QVector<quint16>>();
    } else {
        QString writeData = propertyValue("writeData").toString();
        QStringList parts = writeData.split(",", Qt::SkipEmptyParts);
        for (const QString& part : parts) {
            values.append(part.trimmed().toUShort());
        }
    }
    
    if (values.isEmpty()) {
        setResultData("error", "无数据可写入");
        setStatus(ToolStatus::NG);
        return false;
    }
    
    // 构建Modbus请求
    QByteArray request;
    quint8 functionCode = (dataType == 0) ? 0x06 : 0x05; // 保持寄存器或线圈
    request.append((char)slaveId);
    request.append((char)functionCode);
    request.append((char)((startAddr >> 8) & 0xFF));
    request.append((char)(startAddr & 0xFF));
    request.append((char)((values[0] >> 8) & 0xFF));
    request.append((char)(values[0] & 0xFF));
    
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
        if (response.size() >= 8) {
            setResultData("status", "写入成功");
            setResultData("address", startAddr);
            setResultData("value", values[0]);
            setStatus(ToolStatus::OK);
            return true;
        }
    }
    
    setResultData("error", "写入超时或响应错误");
    setStatus(ToolStatus::NG);
    return false;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ModbusWrite, "MB写数据", VisionInspector::ToolCategory::Communication)
