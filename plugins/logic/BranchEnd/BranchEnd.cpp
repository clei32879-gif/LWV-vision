/** @file BranchEnd.cpp - 分支结束工具实现 */
#include "BranchEnd.h"
#include "../../../src/engine/ToolRegistry.h"

namespace VisionInspector {

PropertyDefList BranchEnd::propertyDefs() const {
    return {
        PropertyDef::intProp("branchId", "所属分支号(0-7)", 0, 0, 7, "归属"),
    };
}

bool BranchEnd::execute(ToolContext& context) {
    const int selfIdx = context.getInt("__engine_index", -1);
    const int myId = propertyValue("branchId").toInt();
    const int currentBranch = context.getInt("__branch_current", -1);

    setResultData("branchId", myId);
    setResultData("executingBranch", currentBranch);

    if (!context.getBool("__branch_active", false) || currentBranch != myId) {
        // 顺序经过了非当前分支的分支结束锚 → 继续走 (case 穿过其他分支壳)
        setStatus(ToolStatus::OK);
        return true;
    }

    // 命中当前分支结束 → 跳到公共路径。
    // 目标: __branch_tail (选择分支写入的流程尾部索引) — 公共尾部工具排在全部分支之后;
    // 若尾部越界则顺序结束本流程。支持公共尾部与"无尾部"两种编排。
    int ret = context.getInt("__branch_tail", -1);
    const int toolCount = context.getInt("__engine_tool_count", -1);
    if (ret >= 0 && toolCount >= 0 && ret >= toolCount) {
        // 尾部索引越界 = 没有公共尾部工具, 直接顺序结束
        context.setData("__branch_active", false);
        setResultData("jumpedBack", false);
        setResultData("flowEnd", true);
        setStatus(ToolStatus::OK);
        return true;
    }
    if (ret >= 0) {
        context.setData("__next_index", ret);
        context.setMessage(3);
        context.setData("__branch_active", false);
        setResultData("jumpedBack", true);
    }
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(BranchEnd, "分支结束", VisionInspector::ToolCategory::Logic)
