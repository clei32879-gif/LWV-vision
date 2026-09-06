/**
 * @file PlcLink.cpp
 * @brief PLC联动工具实现
 */

#include "PlcLink.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/hal/ModbusTcpMaster.h"
#include "../../../src/hal/ModbusRtuMaster.h"

namespace VisionInspector {

PropertyDefList PlcLink::propertyDefs() const {
    return {
        // ---- 通讯 ----
        PropertyDef::enumProp("commMode", "通讯方式", {"Modbus TCP", "Modbus RTU(串口)"}, 0, "连接"),
        PropertyDef::stringProp("host", "PLC地址", "192.168.1.10", "连接"),
        PropertyDef::intProp("port", "端口", 502, 1, 65535, "连接"),
        PropertyDef::stringProp("serialPort", "串口号", "COM1", "连接"),
        PropertyDef::intProp("baudRate", "波特率", 9600, 0, 100000000, "连接"),
        PropertyDef::intProp("dataBits", "数据位", 8, 5, 8, "连接"),
        PropertyDef::intProp("stopBits", "停止位", 1, 1, 2, "连接"),
        PropertyDef::enumProp("parity", "校验", {"无", "偶", "奇"}, 0, "连接"),
        PropertyDef::intProp("slaveId", "从站地址", 1, 1, 247, "连接"),
        PropertyDef::intProp("timeoutMs", "超时(ms)", 1000, 100, 10000, "连接"),

        // ---- 操作 ----
        PropertyDef::enumProp("operation", "操作",
            {
                "启动设备(D6=1)",
                "停止设备(D6=2)",
                "产量清零(D6=3)",
                "读设备状态",
                "读产量统计",
                "读相机位置",
                "相机手动拍照",
                "手动OK剔除",
                "手动NG剔除",
                "设置手动速度",
                "设置自动速度",
                "相机使能/禁用",
                "读剔除位置",
                "读良率与UPH",
                "轴正反转点动",
                "写运行参数(吹气/延时)",
            }, 3, "操作"),
        PropertyDef::intProp("cameraIndex", "相机编号(1-8)", 1, 1, 8, "操作"),
        PropertyDef::intProp("speedValue", "速度值", 500, 0, 100000, "操作"),
        PropertyDef::enumProp("enableState", "使能状态", {"禁用", "使能"}, 1, "操作"),
        PropertyDef::enumProp("axisDir", "轴动作",
            {"正转开(M50=1)", "正转停(M50=0)", "反转开(M52=1)", "反转停(M52=0)"}, 0, "操作"),
        PropertyDef::enumProp("paramSelect", "运行参数",
            {"OK吹气时间(HD18)", "NG吹气时间(HD14)", "停止延时(HD110)", "无料报警延时(HD6)"}, 0, "操作"),
    };
}

bool PlcLink::execute(ToolContext& context) {
    const int commMode = propertyValue("commMode").toInt();
    const int slaveId = propertyValue("slaveId").toInt();
    const int operation = propertyValue("operation").toInt();
    QString err;

    // ---- 建立/获取主站连接 (TCP或RTU) ----
    ModbusTcpMaster* tcp = nullptr;
    ModbusRtuMaster* rtu = nullptr;
    if (commMode == 0) {
        const QString host = propertyValue("host").toString();
        const quint16 port = quint16(propertyValue("port").toInt());
        tcp = ModbusTcpMaster::acquire(host, port, &err);
    } else {
        ModbusRtuMaster::SerialParams sp;
        sp.portName = propertyValue("serialPort").toString();
        sp.baudRate = propertyValue("baudRate").toInt();
        sp.dataBits = propertyValue("dataBits").toInt();
        sp.stopBits = propertyValue("stopBits").toInt();
        // M-39修复: parity是枚举索引, 需映射为"无/偶/奇"
        const int parityIdx = propertyValue("parity").toInt();
        sp.parity = QStringList{"无", "偶", "奇"}.value(parityIdx, "无");
        sp.timeoutMs = propertyValue("timeoutMs").toInt();
        rtu = ModbusRtuMaster::acquire(sp, &err);
    }

    // 统一读寄存器/写寄存器/写线圈的助手
    auto readRegs = [&](int addr, int count, QVector<quint16>* out) -> bool {
        if (tcp) { *out = tcp->readRegisters(3, addr, count, slaveId, &err); return !out->isEmpty(); }
        *out = rtu->readRegisters(3, addr, count, slaveId, &err); return !out->isEmpty();
    };
    auto writeReg = [&](int addr, quint16 v) -> bool {
        if (tcp) return tcp->writeRegister(addr, v, slaveId, &err);
        return rtu->writeRegister(addr, v, slaveId, &err);
    };
    auto writeCoil = [&](int addr, bool on) -> bool {
        if (tcp) return tcp->writeCoil(addr, on, slaveId, &err);
        return rtu->writeCoil(addr, on, slaveId, &err);
    };

    // ---- 按操作执行 ----
    switch (operation) {
        case 0: // 启动设备
            if (!writeReg(kRegD6Cmd, 1)) return fail(err);
            setResultData("command", "启动");
            break;
        case 1: // 停止设备
            if (!writeReg(kRegD6Cmd, 2)) return fail(err);
            setResultData("command", "停止");
            break;
        case 2: // 产量清零
            if (!writeReg(kRegD6Cmd, 3)) return fail(err);
            setResultData("command", "产量清零");
            break;
        case 3: { // 读设备状态: 运行/停止
            QVector<quint16> r;
            if (!readRegs(kRegD6Cmd, 1, &r)) return fail(err);
            const int st = r[0];
            const QString text = (st == 1) ? "运行中" : (st == 2 ? "停止中" : "待机");
            setResultData("deviceState", st);
            setResultData("deviceStateText", text);
            context.setData("plc.deviceState", st);
            context.setData("plc.deviceStateText", text);
            break;
        }
        case 4: { // 读产量统计
            QVector<quint16> r;
            if (!readRegs(kRegOKCount, 4, &r)) return fail(err);
            const int okCount = r[0], ngCount = r[1], total = r[2], retest = r[3];
            setResultData("okCount", okCount);
            setResultData("ngCount", ngCount);
            setResultData("totalCount", total);
            setResultData("retestCount", retest);
            context.setData("plc.okCount", okCount);
            context.setData("plc.ngCount", ngCount);
            context.setData("plc.totalCount", total);
            context.setData("plc.retestCount", retest);
            // 良率 (HD122) 与 UPH (D196) 可后续单独读
            break;
        }
        case 5: { // 读相机位置 HD200..HD214 (8个)
            QVector<quint16> r;
            if (!readRegs(kRegCam1Pos, 8, &r)) return fail(err);
            QStringList pos;
            for (int i = 0; i < 8; ++i) {
                setResultData(QString("camPos%1").arg(i + 1), r[i]);
                context.setData(QString("plc.camPos%1").arg(i + 1), r[i]);
                pos << QString::number(r[i]);
            }
            setResultData("camPositions", pos.join(","));
            break;
        }
        case 6: { // 相机手动拍照: M121/M123/... 步进2
            const int cam = propertyValue("cameraIndex").toInt();
            const int coil = kCoilManualShotBase + (cam - 1) * 2;
            if (!writeCoil(coil, true)) return fail(err);
            // 保持一小段脉冲后复位 (由PLC程序内部边沿处理, 这里直接写1即可)
            setResultData("cameraShot", cam);
            break;
        }
        case 7: // 手动OK剔除
            if (!writeCoil(kCoilManualOK, true)) return fail(err);
            setResultData("manualAction", "OK剔除");
            break;
        case 8: // 手动NG剔除
            if (!writeCoil(kCoilManualNG, true)) return fail(err);
            setResultData("manualAction", "NG剔除");
            break;
        case 9: // 设置手动速度 HD0
            if (!writeReg(kRegManualSpd, quint16(propertyValue("speedValue").toInt())))
                return fail(err);
            setResultData("manualSpeed", propertyValue("speedValue").toInt());
            break;
        case 10: // 设置自动速度 HD8
            if (!writeReg(kRegAutoSpd, quint16(propertyValue("speedValue").toInt())))
                return fail(err);
            setResultData("autoSpeed", propertyValue("speedValue").toInt());
            break;
        case 11: { // 相机使能/禁用 HM1..HM8
            const int cam = propertyValue("cameraIndex").toInt();
            const int en = propertyValue("enableState").toInt();
            if (!writeReg(kRegHM1 + cam - 1, quint16(en))) return fail(err);
            setResultData("cameraEnable", en);
            setResultData("cameraIndex", cam);
            break;
        }
        case 12: { // 读剔除位置 HD400/HD402
            QVector<quint16> r;
            if (!readRegs(kRegOKBlowPos, 2, &r)) return fail(err);
            setResultData("okBlowPos", r[0]);
            setResultData("ngBlowPos", r[1]);
            context.setData("plc.okBlowPos", r[0]);
            context.setData("plc.ngBlowPos", r[1]);
            break;
        }
        case 13: { // 读良率 HD122 与 UPH D196
            QVector<quint16> y, u;
            if (!readRegs(kRegYield, 1, &y)) return fail(err);
            if (!readRegs(kRegUph, 1, &u)) return fail(err);
            setResultData("yield", y[0]);
            setResultData("uph", u[0]);
            context.setData("plc.yield", y[0]);
            context.setData("plc.uph", u[0]);
            break;
        }
        case 14: { // 轴正反转点动 M50/M52 (伺服/转盘手动微动)
            const int dir = propertyValue("axisDir").toInt();
            const int coil = (dir <= 1) ? kCoilFwd : kCoilRev;
            const bool on = (dir == 0 || dir == 2);
            if (!writeCoil(coil, on)) return fail(err);
            setResultData("axisAction", (dir <= 1) ? "正转" : "反转");
            setResultData("axisOn", on);
            break;
        }
        case 15: { // 写运行参数: 吹气时间/停止延时/无料报警延时
            const int sel = propertyValue("paramSelect").toInt();
            const quint16 val = quint16(propertyValue("speedValue").toInt());
            static constexpr int kParamRegs[4] = { kRegOKBlowTime, kRegNGBlowTime,
                                                   kRegStopDelay, kRegNoMatDelay };
            if (!writeReg(kParamRegs[sel], val)) return fail(err);
            setResultData("paramIndex", sel);
            setResultData("paramValue", val);
            break;
        }
        default:
            return fail(QStringLiteral("未知操作"));
    }

    setStatus(ToolStatus::OK);
    return true;
}

bool PlcLink::fail(const QString& err) {
    setResultData("error", err);
    setStatus(ToolStatus::NG);
    return false;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(PlcLink, "PLC联动", VisionInspector::ToolCategory::Communication)
