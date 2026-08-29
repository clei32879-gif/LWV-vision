/**
 * @file IIODriver.h
 * @brief IO驱动接口 (输入输出控制)
 *
 * 用于控制: 吹气电磁阀、传感器信号、光源等
 * 通常通过PLC的IO或独立IO模块实现
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace VisionInspector {

class IIODriver : public QObject {
    Q_OBJECT

public:
    IIODriver(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IIODriver() = default;

    virtual QString driverName() const = 0;
    virtual bool connect(const QString& params) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    /** 读输入 */
    virtual bool readInput(int channel, bool& value) = 0;
    virtual bool readAllInputs(QVector<bool>& values) = 0;

    /** 写输出 */
    virtual bool writeOutput(int channel, bool value) = 0;
    virtual bool writeAllOutputs(const QVector<bool>& values) = 0;

    /** 脉冲输出 (控制电磁阀吹气) */
    virtual bool pulseOutput(int channel, int durationMs) = 0;

    /** 获取输入通道数 */
    virtual int inputCount() const = 0;

    /** 获取输出通道数 */
    virtual int outputCount() const = 0;

signals:
    void inputChanged(int channel, bool value);
    void errorOccurred(const QString& error);
};

} // namespace VisionInspector
