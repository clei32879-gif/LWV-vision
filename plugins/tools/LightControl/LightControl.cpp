/** @file LightControl.cpp - 光源控制工具实现
 *
 *  两种模式:
 *    mode=0 OPT标准帧: 奥普特/通用四通道控制器常见帧形态
 *      点亮:  AA <chn> <bright HEX> <XOR校验>  (chn: 1-4, bright: 0x00-0xFF)
 *      熄灭:  AA <chn> 00 <XOR校验>
 *    mode=1 自定义HEX: 用户直接给十六进制串(可含空格), 原样发出
 *  所有模式发送后延时等待控制器响应 (RS485 半双工), 并读回响应计数。
 */
#include "LightControl.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>

namespace VisionInspector {

PropertyDefList LightControl::propertyDefs() const {
    return {
        PropertyDef::stringProp("serialPort", "串口号", "COM3", "连接"),
        PropertyDef::intProp("baudRate", "波特率", 9600, 1200, 115200, "连接"),
        PropertyDef::intProp("timeoutMs", "响应超时(ms)", 200, 50, 5000, "连接"),
        PropertyDef::enumProp("mode", "指令模式", {"OPT四通道标准帧", "自定义HEX指令"}, 0, "指令"),
        PropertyDef::intProp("channel", "通道(1-4)", 1, 1, 4, "指令"),
        PropertyDef::intProp("brightness", "亮度(0-255)", 128, 0, 255, "指令"),
        PropertyDef::enumProp("action", "动作",
            {"点亮(设亮度)", "熄灭", "全部点亮(同亮度)", "全部熄灭"}, 0, "指令"),
        PropertyDef::stringProp("customHex", "自定义HEX指令", "AA 01 80 AA", "指令"),
    };
}

static QByteArray xorFrame(quint8 chn, quint8 bright) {
    // OPT 常见帧: 帧头AA + 通道 + 亮度 + 异或校验
    QByteArray f;
    f.append(char(0xAA));
    f.append(char(chn));
    f.append(char(bright));
    quint8 x = 0xAA ^ chn ^ bright;
    f.append(char(x));
    return f;
}

bool LightControl::execute(ToolContext& context) {
    const QString portName = propertyValue("serialPort").toString().trimmed();
    const int baud = propertyValue("baudRate").toInt();
    const int timeout = propertyValue("timeoutMs").toInt();
    const int mode = propertyValue("mode").toInt();
    const int chn = propertyValue("channel").toInt();
    const int bright = propertyValue("brightness").toInt();
    const int action = propertyValue("action").toInt();

    // 组帧
    QByteArray frame;
    QStringList logDesc;
    if (mode == 1) {
        // 自定义HEX: "AA 01 80 AA" → 字节
        const QString hex = propertyValue("customHex").toString();
        const QString cleaned = QString(hex).remove(' ').remove(':');
        if (cleaned.isEmpty() || cleaned.size() % 2 != 0) {
            setResultData("error", QStringLiteral("HEX指令格式错误(需偶数位十六进制): %1").arg(hex));
            setStatus(ToolStatus::NG); return false;
        }
        frame = QByteArray::fromHex(cleaned.toLatin1());
        logDesc << QStringLiteral("自定义HEX %1 字节").arg(frame.size());
    } else {
        if (action == 0) frame = xorFrame(quint8(chn), quint8(bright));        // 点亮
        else if (action == 1) frame = xorFrame(quint8(chn), 0);                 // 熄灭
        else if (action == 2) {                                                 // 全亮
            for (int c = 1; c <= 4; ++c) frame += xorFrame(quint8(c), quint8(bright));
        } else {                                                                // 全灭
            for (int c = 1; c <= 4; ++c) frame += xorFrame(quint8(c), 0);
        }
        logDesc << QStringLiteral("OPT帧 通道%1 亮度%2 动作%3").arg(chn).arg(bright).arg(action);
    }
    if (frame.isEmpty()) {
        setResultData("error", "指令帧为空");
        setStatus(ToolStatus::NG); return false;
    }

    // 打开串口 (独占即用即关, 与 ModbusRtuMaster 连接池隔离 — 光源控制器通常独占串口)
    QSerialPort serial;
    serial.setPortName(portName);
    serial.setBaudRate(baud);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);
    if (!serial.open(QIODevice::ReadWrite)) {
        setResultData("error", QStringLiteral("串口打开失败: %1 (%2)")
            .arg(portName, serial.errorString()));
        setStatus(ToolStatus::NG); return false;
    }

    serial.write(frame);
    if (!serial.waitForBytesWritten(500)) {
        setResultData("error", "指令写出超时");
        setStatus(ToolStatus::NG); return false;
    }

    // 读响应 (半双工等待)
    int respBytes = 0;
    if (serial.waitForReadyRead(timeout)) {
        QByteArray resp = serial.readAll();
        respBytes = resp.size();
        // 多字节帧可能分片到达, 再等一小段
        if (serial.waitForReadyRead(50)) resp += serial.readAll();
        setResultData("responseHex", QString::fromLatin1(resp.toHex(' ').toUpper()));
    }
    serial.close();

    setResultData("sentHex", QString::fromLatin1(frame.toHex(' ').toUpper()));
    setResultData("responseBytes", respBytes);
    setResultData("port", portName);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(LightControl, "光源控制", VisionInspector::ToolCategory::Communication)
