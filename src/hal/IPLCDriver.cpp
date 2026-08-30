/**
 * @file IPLCDriver.cpp
 * @brief PLC驱动接口的便捷方法默认实现 (基于核心Modbus读写方法)
 *
 * readRegister/writeRegister/readBool/writeBool/readInt32/writeInt32/
 * readFloat/writeFloat 均由派生类实现的核心方法组合而成,
 * 派生类无需重复实现。
 */

#include "IPLCDriver.h"
#include <cstring>

namespace VisionInspector {

quint16 IPLCDriver::readRegister(int address) {
    QVector<quint16> values;
    if (!readHoldingRegisters(address, 1, values) || values.size() != 1)
        return 0;
    return values[0];
}

bool IPLCDriver::writeRegister(int address, quint16 value) {
    QVector<quint16> values = {value};
    return writeHoldingRegisters(address, values);
}

bool IPLCDriver::readBool(int address) {
    QVector<bool> values;
    if (!readCoils(address, 1, values) || values.size() != 1)
        return false;
    return values[0];
}

bool IPLCDriver::writeBool(int address, bool value) {
    QVector<bool> values = {value};
    return writeCoils(address, values);
}

qint32 IPLCDriver::readInt32(int address) {
    QVector<quint16> values;
    if (!readHoldingRegisters(address, 2, values) || values.size() != 2)
        return 0;
    const quint32 lo = values[0];
    const quint32 hi = values[1];
    return qint32((hi << 16) | lo);
}

bool IPLCDriver::writeInt32(int address, qint32 value) {
    QVector<quint16> values = {
        quint16(value & 0xFFFF),
        quint16((quint32(value) >> 16) & 0xFFFF),
    };
    return writeHoldingRegisters(address, values);
}

float IPLCDriver::readFloat(int address) {
    QVector<quint16> values;
    if (!readHoldingRegisters(address, 2, values) || values.size() != 2)
        return 0.0f;
    const quint32 raw = (quint32(values[1]) << 16) | values[0];
    float f = 0.0f;
    memcpy(&f, &raw, sizeof(f));
    return f;
}

bool IPLCDriver::writeFloat(int address, float value) {
    quint32 raw = 0;
    memcpy(&raw, &value, sizeof(raw));
    QVector<quint16> values = {
        quint16(raw & 0xFFFF),
        quint16((raw >> 16) & 0xFFFF),
    };
    return writeHoldingRegisters(address, values);
}

} // namespace VisionInspector
