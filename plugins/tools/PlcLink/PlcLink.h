/**
 * @file PlcLink.h
 * @brief PLC联动工具 — LW Vision 作为上位机与信捷XD5等PLC协同, 实现"筛选物料"
 *
 * 面向 8工位转盘筛选机 的上位机联动:
 *   - 内置信捷XD5契约地址预设 (与PLC梯形图/上位机通讯契约一致)
 *   - 上位机命令: 启动/停止/产量清零 (写D6)
 *   - 状态与统计: 读设备状态/OK产量/NG产量/总产量/重测/良率/UPH
 *   - 相机联动: 相机1-8位置读取、手动拍照、相机使能(HM)
 *   - 剔除联动: 手动OK/NG剔除 (写M116/M117)
 *   - 速度设置: 手动速度HD0 / 自动速度HD8
 * 通讯支持 Modbus TCP 与 Modbus RTU(串口) 双通道。
 *
 * 契约地址映射 (已验证自洽): 保持寄存器 = 41088 + D号; 线圈 = M号。
 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class PlcLink : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("PlcLink"); }
    QString displayName() const override { return QStringLiteral("PLC联动"); }
    ToolCategory category() const override { return ToolCategory::Communication; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;

private:
    /** 失败处理: 记录错误并返回false */
    bool fail(const QString& err);

private:
    /** 契约内置信捷XD5 8工位转盘筛选机 地址表 (Modbus地址, 0起) */
    static constexpr int kRegD6Cmd     = 41094;   // D6 上位机命令: 1启动 2停止 3产量清零
    static constexpr int kRegOKCount   = 41258;   // HD170 OK产量
    static constexpr int kRegNGCount   = 41260;   // HD172 NG产量
    static constexpr int kRegTotal     = 41262;   // HD174 总产量
    static constexpr int kRegRetest    = 41264;   // HD176 重测产品数
    static constexpr int kRegYield     = 41210;   // HD122 良率
    static constexpr int kRegUph       = 41284;   // D196 产品UPH
    static constexpr int kRegCam1Pos   = 41288;   // HD200 相机1位置 (1-8: 41288..41302)
    static constexpr int kRegOKBlowPos = 41488;   // HD400 OK剔除位置
    static constexpr int kRegNGBlowPos = 41490;   // HD402 NG剔除位置
    static constexpr int kRegManualSpd = 41088;   // HD0 手动速度
    static constexpr int kRegAutoSpd   = 41096;   // HD8 自动速度
    static constexpr int kRegHM1       = 49409;   // HM1 相机1使能 (1-8: 49409..49416)
    static constexpr int kCoilFwd      = 50;      // M50 轴正转
    static constexpr int kCoilRev      = 52;      // M52 轴反转
    static constexpr int kCoilManualShotBase = 121; // M121 相机1手动拍照 (步进2)
    static constexpr int kCoilManualOK  = 116;    // M116 手动OK剔除
    static constexpr int kCoilManualNG  = 117;    // M117 手动NG剔除
};
} // namespace VisionInspector
