#include "SerialPort.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QSerialPort>
#include <QSerialPortInfo>

namespace VisionInspector {

PropertyDefList SerialPortTool::propertyDefs() const {
    return {
        PropertyDef::enumProp("port", "端口选择", {"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8"}, 0),
        PropertyDef::enumProp("baudRate", "波特率", {"9600", "19200", "38400", "57600", "115200"}, 0),
        PropertyDef::enumProp("dataBits", "数据位", {"5", "6", "7", "8"}, 3),
        PropertyDef::enumProp("parity", "奇偶校验", {"无", "偶数", "奇数"}, 0),
        PropertyDef::enumProp("stopBits", "停止位", {"1", "1.5", "2"}, 0),
        PropertyDef::enumProp("action", "操作", {"打开", "关闭"}, 0),
    };
}

bool SerialPortTool::execute(ToolContext& context) {
    int action = propertyValue("action").toInt();
    void* ptr = context.getData("serialPort").value<void*>();
    QSerialPort* port = static_cast<QSerialPort*>(ptr);
    
    if (action == 0) {
        // 打开串口
        if (port && port->isOpen()) {
            setResultData("status", "串口已打开");
            setStatus(ToolStatus::OK);
            return true;
        }
        
        port = new QSerialPort();
        
        // 设置端口名
        int portIdx = propertyValue("port").toInt();
        QStringList portNames = {"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8"};
        port->setPortName(portNames[portIdx]);
        
        // 设置波特率
        int baudIdx = propertyValue("baudRate").toInt();
        QList<qint32> baudRates = {9600, 19200, 38400, 57600, 115200};
        port->setBaudRate(baudRates[baudIdx]);
        
        // 设置数据位
        int dataIdx = propertyValue("dataBits").toInt();
        QList<QSerialPort::DataBits> dataBits = {
            QSerialPort::Data5, QSerialPort::Data6, 
            QSerialPort::Data7, QSerialPort::Data8
        };
        port->setDataBits(dataBits[dataIdx]);
        
        // 设置校验位
        int parityIdx = propertyValue("parity").toInt();
        QList<QSerialPort::Parity> parities = {
            QSerialPort::NoParity, QSerialPort::EvenParity, QSerialPort::OddParity
        };
        port->setParity(parities[parityIdx]);
        
        // 设置停止位
        int stopIdx = propertyValue("stopBits").toInt();
        QList<QSerialPort::StopBits> stopBits = {
            QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop
        };
        port->setStopBits(stopBits[stopIdx]);
        
        if (port->open(QIODevice::ReadWrite)) {
            context.setData("serialPort", QVariant::fromValue((void*)port));
            setResultData("status", "串口已打开");
            setResultData("port", portNames[portIdx]);
            setResultData("baudRate", baudRates[baudIdx]);
            setStatus(ToolStatus::OK);
            return true;
        } else {
            delete port;
            setResultData("error", QString("无法打开串口: %1").arg(port->errorString()));
            setStatus(ToolStatus::NG);
            return false;
        }
    } else {
        // 关闭串口
        if (port && port->isOpen()) {
            port->close();
            context.setData("serialPort", QVariant());
            setResultData("status", "串口已关闭");
            setStatus(ToolStatus::OK);
            return true;
        }
        setResultData("status", "串口未打开");
        setStatus(ToolStatus::OK);
        return true;
    }
}

} // namespace VisionInspector

VI_REGISTER_TOOL(SerialPortTool, "串行口", VisionInspector::ToolCategory::Communication)
