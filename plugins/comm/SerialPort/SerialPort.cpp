#include "SerialPort.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QRegularExpression>

namespace VisionInspector {

PropertyDefList SerialPort::propertyDefs() const {
    return {
        PropertyDef::enumProp("portName", "端口号",
            {"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8"}, 0),
        PropertyDef::enumProp("baudRate", "波特率",
            {"9600", "19200", "38400", "57600", "115200"}, 0),
        PropertyDef::enumProp("dataBits", "数据位", {"5", "6", "7", "8"}, 3),
        PropertyDef::enumProp("stopBits", "停止位", {"1", "1.5", "2"}, 0),
        PropertyDef::enumProp("parity", "校验", {"无", "偶数", "奇数"}, 0),
        PropertyDef::enumProp("mode", "收发模式", {"ASCII", "HEX"}, 0),
        PropertyDef::enumProp("action", "操作", {"打开", "关闭", "发送", "接收"}, 0),
        PropertyDef::stringProp("sendData", "发送数据", ""),
    };
}

bool SerialPort::execute(ToolContext& context) {
    int action = propertyValue("action").toInt();
    int mode = propertyValue("mode").toInt();
    void* ptr = context.getData("__SerialPort").value<void*>();
    QSerialPort* port = static_cast<QSerialPort*>(ptr);

    if (action == 0) {
        if (port && port->isOpen()) {
            setResultData("status", "串口已打开");
            setStatus(ToolStatus::OK);
            return true;
        }
        port = new QSerialPort();

        int portIdx = propertyValue("portName").toInt();
        QStringList portNames = {"COM1", "COM2", "COM3", "COM4",
                                "COM5", "COM6", "COM7", "COM8"};
        port->setPortName(portNames[portIdx]);

        int baudIdx = propertyValue("baudRate").toInt();
        QList<qint32> baudRates = {9600, 19200, 38400, 57600, 115200};
        port->setBaudRate(baudRates[baudIdx]);

        int dataIdx = propertyValue("dataBits").toInt();
        QList<QSerialPort::DataBits> dataBits = {
            QSerialPort::Data5, QSerialPort::Data6,
            QSerialPort::Data7, QSerialPort::Data8
        };
        port->setDataBits(dataBits[dataIdx]);

        int stopIdx = propertyValue("stopBits").toInt();
        QList<QSerialPort::StopBits> stopBits = {
            QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop
        };
        port->setStopBits(stopBits[stopIdx]);

        int parityIdx = propertyValue("parity").toInt();
        QList<QSerialPort::Parity> parities = {
            QSerialPort::NoParity, QSerialPort::EvenParity, QSerialPort::OddParity
        };
        port->setParity(parities[parityIdx]);

        if (port->open(QIODevice::ReadWrite)) {
            context.setData("__SerialPort", QVariant::fromValue((void*)port));
            setResultData("status", "串口已打开");
            setResultData("port", portNames[portIdx]);
            setResultData("baudRate", baudRates[baudIdx]);
            setStatus(ToolStatus::OK);
            return true;
        } else {
            QString err = port->errorString();
            delete port;
            setResultData("error", QString("串口打开失败: %1").arg(err));
            setStatus(ToolStatus::NG);
            return false;
        }
    } else if (action == 1) {
        if (port && port->isOpen()) {
            port->close();
            context.setData("__SerialPort", QVariant());
            setResultData("status", "串口已关闭");
            setStatus(ToolStatus::OK);
            return true;
        }
        setResultData("status", "串口未打开");
        setStatus(ToolStatus::OK);
        return true;
    } else if (action == 2) {
        if (!port || !port->isOpen()) {
            setResultData("error", "串口未打开");
            setStatus(ToolStatus::NG);
            return false;
        }
        QString sendStr = propertyValue("sendData").toString();
        if (sendStr.isEmpty()) {
            setResultData("error", "发送数据为空");
            setStatus(ToolStatus::NG);
            return false;
        }
        QByteArray data;
        if (mode == 0) {
            data = sendStr.toUtf8();
        } else {
            QString hex = sendStr;
            hex.remove(QRegularExpression("\\s+"));
            data = QByteArray::fromHex(hex.toUtf8());
        }
        qint64 written = port->write(data);
        if (written == data.size()) {
            setResultData("status", "发送成功");
            setResultData("sentBytes", (int)written);
            setStatus(ToolStatus::OK);
        } else {
            setResultData("error", QString("发送失败, 已发送%1字节").arg(written));
            setStatus(ToolStatus::NG);
        }
        port->waitForBytesWritten(1000);
        return true;
    } else if (action == 3) {
        if (!port || !port->isOpen()) {
            setResultData("error", "串口未打开");
            setStatus(ToolStatus::NG);
            return false;
        }
        if (port->waitForReadyRead(1000)) {
            QByteArray data = port->readAll();
            QString result;
            if (mode == 0) {
                result = QString::fromUtf8(data);
            } else {
                result = QString::fromLatin1(data.toHex(' ').toUpper());
            }
            setResultData("receivedData", result);
            setResultData("receivedBytes", (int)data.size());
            setResultData("status", "接收成功");
            setStatus(ToolStatus::OK);
        } else {
            setResultData("receivedData", QString());
            setResultData("receivedBytes", 0);
            setResultData("status", "接收超时");
            setStatus(ToolStatus::OK);
        }
        return true;
    }

    setResultData("error", "未知操作");
    setStatus(ToolStatus::NG);
    return false;
}

VI_REGISTER_TOOL(SerialPort, "串口通讯", VisionInspector::ToolCategory::Communication)
} // namespace VisionInspector
