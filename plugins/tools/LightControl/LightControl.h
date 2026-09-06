/** @file LightControl.h - 光源控制工具 (对标 CKVision 光源控制, P0)
 *
 *  串口光源控制器 (奥普特/CCS/康视达等主流协议形态):
 *    - 4通道亮度设定 (0-255) 与开关
 *    - 通道切换 (产线换型时选通道)
 *    - 自定义十六进制指令 (非标控制器直接发原始帧)
 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

class LightControl : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("LightControl"); }
    QString displayName() const override { return QStringLiteral("光源控制"); }
    ToolCategory category() const override { return ToolCategory::Communication; }
    QString description() const override { return QStringLiteral(
        "串口控制光源控制器通道亮度与开关, 支持自定义HEX指令；用于产线调光换型"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
