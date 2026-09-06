/** @file ExecuteFlow.h - 执行流程工具 (对标 CKVision 执行流程, P0: 子流程复用) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

/**
 * 执行流程: 在当前流程执行中, 按名调用另一个流程 (子流程复用, 结果写入当前上下文)
 * 防递归: 引擎内 m_subFlowDepth 计数, 超过 8 层 NG。
 */
class ExecuteFlow : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ExecuteFlow"); }
    QString displayName() const override { return QStringLiteral("执行流程"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral(
        "按名调用另一个流程作为子流程执行, 结果写入当前上下文；用于流程复用与模块化"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
