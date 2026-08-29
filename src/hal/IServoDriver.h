/**
 * @file IServoDriver.h
 * @brief 伺服驱动抽象接口
 *
 * 用于控制伺服电机 (驱动圆形玻璃盘旋转)。
 * 实际使用中通常通过PLC间接控制伺服,
 * 但也预留直接控制接口以备扩展。
 */

#pragma once

#include <QObject>
#include <QString>

namespace VisionInspector {

enum class ServoControlMode {
    Position,   // 位置控制 (脉冲+方向)
    Speed,      // 速度控制
    Torque,     // 扭矩控制
    Bus,        // 总线控制 (EtherCAT等)
};

struct ServoParams {
    ServoControlMode mode = ServoControlMode::Position;
    int pulsePerRev = 10000;     // 每转脉冲数
    double gearRatio = 1.0;      // 减速比
    int speed = 500;             // 速度 (rpm或脉冲/秒)
    int accel = 1000;            // 加速度
    int decel = 1000;            // 减速度
};

class IServoDriver : public QObject {
    Q_OBJECT

public:
    IServoDriver(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IServoDriver() = default;

    virtual QString driverName() const = 0;
    virtual bool connect(const QString& port) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    /** 伺服使能 */
    virtual bool enable() = 0;

    /** 伺服使能取消 */
    virtual bool disable() = 0;

    /** 是否已使能 */
    virtual bool isEnabled() const = 0;

    /** 获取当前位置 (脉冲) */
    virtual qint64 currentPosition() const = 0;

    /** 获取当前转速 (rpm) */
    virtual double currentSpeed() const = 0;

    /** 相对移动 (脉冲) */
    virtual bool moveRelative(qint64 pulses, int speed = 0) = 0;

    /** 绝对移动 (脉冲) */
    virtual bool moveAbsolute(qint64 targetPos, int speed = 0) = 0;

    /** 旋转指定圈数 */
    virtual bool rotateRevolutions(double revolutions, int speed = 0) = 0;

    /** 持续旋转 (速度模式) */
    virtual bool rotateContinuous(int rpm) = 0;

    /** 停止运动 */
    virtual bool stop() = 0;

    /** 回零 */
    virtual bool goHome(int speed = 0) = 0;

    /** 设置参数 */
    virtual bool setParams(const ServoParams& params) = 0;

    /** 获取参数 */
    virtual ServoParams getParams() const = 0;

signals:
    void positionChanged(qint64 position);
    void speedChanged(double rpm);
    void connectionChanged(bool connected);
    void errorOccurred(const QString& error);
};

} // namespace VisionInspector
