/** @file ExecuteFlow.cpp - 执行流程工具实现 */
#include "ExecuteFlow.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/engine/FlowEngine.h"
#include "../../../src/utils/Logger.h"

namespace VisionInspector {

PropertyDefList ExecuteFlow::propertyDefs() const {
    return {
        PropertyDef::stringProp("flowName", "子流程名", "", "调用"),
        PropertyDef::boolProp("passCurrentImage", "传入当前图像", true, "调用"),
        PropertyDef::boolProp("ngIsNG", "子流程NG则本工具NG", true, "判定"),
    };
}

bool ExecuteFlow::execute(ToolContext& context) {
    FlowEngine* engine = context.flowEngine();
    if (!engine) {
        setResultData("error", "上下文未携带流程引擎");
        setStatus(ToolStatus::NG); return false;
    }
    const QString name = propertyValue("flowName").toString().trimmed();
    if (name.isEmpty()) {
        setResultData("error", "未填写子流程名");
        setStatus(ToolStatus::NG); return false;
    }
    Flow* sub = engine->findFlow(name);
    if (!sub) {
        setResultData("error", QStringLiteral("未找到流程: %1").arg(name));
        setStatus(ToolStatus::NG); return false;
    }
    // 防自递归: 子流程即自己所在流程 → 死循环
    if (context.getBool("__engine_in_flow_" + name, false)) {
        setResultData("error", QStringLiteral("禁止递归调用自身流程: %1").arg(name));
        setStatus(ToolStatus::NG); return false;
    }

    const int depth = context.getInt("__subflow_depth", 0);
    if (depth >= 8) {
        setResultData("error", "子流程嵌套超过8层 (疑似递归)");
        setStatus(ToolStatus::NG); return false;
    }

    // 子流程执行: 直接 doExecute (共享当前上下文 = 结果/命名图像互通),
    // 深度+1 防递归; 引擎的 __engine_index 会被子流程覆盖, 保存恢复供父流程消息跳转使用
    const int parentIdx = context.getInt("__engine_index", -1);
    context.setData("__subflow_depth", depth + 1);
    context.setData("__engine_in_flow_" + name, true);
    const int savedMsg = context.message();
    context.setMessage(0);

    VI_LOG_INFO(QString("执行流程: 进入子流程[%1] (深度%2)").arg(name).arg(depth + 1));
    const bool subOk = engine->executeSubFlow(sub, context);

    context.setData("__engine_in_flow_" + name, false);
    context.setData("__subflow_depth", depth);
    context.setMessage(0);              // 丢弃子流程遗留消息
    if (parentIdx >= 0) context.setData("__engine_index", parentIdx);
    if (savedMsg != 0) context.setMessage(savedMsg);

    setResultData("subFlow", name);
    setResultData("subFlowOk", subOk);
    const bool ngAsNG = propertyValue("ngIsNG").toBool();
    const bool ok = ngAsNG ? subOk : true;
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ExecuteFlow, "执行流程", VisionInspector::ToolCategory::Logic)
