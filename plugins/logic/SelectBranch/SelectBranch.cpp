/** @file SelectBranch.cpp - 选择分支工具实现 (消息跳转机制, 与 Loop/LoopEnd 同款) */
#include "SelectBranch.h"
#include "../../../src/engine/ToolRegistry.h"

namespace VisionInspector {

PropertyDefList SelectBranch::propertyDefs() const {
    return {
        PropertyDef::intProp("branchIndex", "选择分支号(0-7)", 0, 0, 7, "选择"),
        // 数据链接支持: branchIndex 可写 "$(工具.键)" 由上游条件计算
        PropertyDef::stringProp("indexSource", "分支号来源(留空用上方数值)", "", "选择"),
    };
}

bool SelectBranch::execute(ToolContext& context) {
    const int selfIdx = context.getInt("__engine_index", -1);
    if (selfIdx < 0) {
        setResultData("error", "无法获取流程索引 (引擎未回写)");
        setStatus(ToolStatus::NG); return false;
    }

    // 分支号: 优先数据链接, 否则用数值属性
    int k = propertyValue("branchIndex").toInt();
    const QString src = propertyValue("indexSource").toString().trimmed();
    if (!src.isEmpty()) {
        bool okNum = false;
        const double v = context.getData(src).toDouble(&okNum);
        if (okNum) k = int(v);
    }

    // 记录分支元数据:
    //   __branch_return: 分支体全部结束后应回到的公共路径起点。
    //   SelectBranch 无法预知分支体长度 → 记 __branch_tail = 流程工具总数 (所有分支排在流程末段),
    //   BranchEnd 命中时跳到 min(尾部索引, 引擎工具数) — 即流程末尾; 公共尾部工具排在全部分支之后。
    context.setData("__branch_tail", context.getInt("__engine_tool_count", selfIdx + 1));
    context.setData("__branch_return", selfIdx + 1);   // 兼容旧约定 (无公共尾部时退化为顺序)
    context.setData("__branch_current", k);   // 供分支结束工具比对
    context.setData("__branch_active", true);

    setResultData("branchIndex", k);
    setResultData("branchStart", selfIdx);
    // 跳过自身下一格的分支壳: case0 从 selfIdx+1 开始 (无 BranchStart 锚, 分支体直接排布)
    context.setData("__next_index", selfIdx + 1);
    context.setMessage(3);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(SelectBranch, "选择分支", VisionInspector::ToolCategory::Logic)
