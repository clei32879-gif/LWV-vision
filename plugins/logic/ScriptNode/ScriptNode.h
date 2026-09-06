/** @file ScriptNode.h - 脚本节点 (对标 CKVision 代码编辑, P0: QJSEngine, 无新依赖) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

/**
 * 脚本节点: 执行 JavaScript (QJSEngine)。
 * 脚本环境预置:
 *   data     — 上下文数据键值对象 (读: data["工具.键"]; 写: data["key"] = v)
 *   globals  — 全局变量对象 (读写)
 *   input    — 输入图像宽高 {width, height} (只读)
 *   log(msg) — 写日志
 * 返回: 脚本最后表达式的布尔值 = 工具 OK/NG (或用 return true/false)
 */
class ScriptNode : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ScriptNode"); }
    QString displayName() const override { return QStringLiteral("脚本节点"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral(
        "执行JavaScript脚本访问上下文数据与全局变量, 返回值决定OK/NG；用于灵活逻辑与批量计算"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
