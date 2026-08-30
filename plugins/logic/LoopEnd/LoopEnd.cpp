/**
 * @file LoopEnd.cpp
 * @brief 循环结束工具 (对标 CKVision 循环工具"结束循环", M-23 修复)
 *
 * 与"循环"(Loop) 配对: Loop 声明循环起点, 本工具判定循环是否结束。
 * - 未结束: 索引+1/递减, 置 message=1 → 引擎回跳到 __loop_start
 * - 已结束 或 收到停止请求(__loop_break): 清除活动循环, 顺序继续
 *
 * 输出: loopIndex / loopComplete / loopBroken
 */
#include "LoopEnd.h"
#include "../../../src/engine/ToolRegistry.h"

namespace VisionInspector {

PropertyDefList LoopEnd::propertyDefs() const {
    return {};   // 无参数, 状态全从上下文读取
}

bool LoopEnd::execute(ToolContext& context) {
    // 记录循环结束索引 (供引擎 message==2 停止循环跳转)
    const int selfIdx = context.getInt("__engine_index", -1);
    if (selfIdx >= 0)
        context.setData("__loop_end", selfIdx);

    // 没有活动循环 → 直接放行
    if (!context.getBool("__loop_active", false)) {
        setResultData("loopComplete", true);
        setResultData("loopBroken", false);
        setStatus(ToolStatus::OK);
        return true;
    }

    // 收到停止循环请求 → 清除循环状态, 顺序继续
    if (context.getBool("__loop_break", false)) {
        context.setData("__loop_active", false);
        context.setData("__loop_break", false);
        setResultData("loopComplete", true);
        setResultData("loopBroken", true);
        setStatus(ToolStatus::OK);
        return true;
    }

    const int mode = context.getInt("__loop_mode", 0);
    const int A = context.getInt("__loop_A", 1);
    const int B = context.getInt("__loop_B", 5);
    const QString key = context.getData("__loop_index_key").toString();
    const QString indexKey = key.isEmpty() ? QStringLiteral("loopIndex") : key;
    int cur = context.getInt(indexKey, A);

    bool finished = false;
    if (mode == 2) {
        // 无限: 索引递增, 永不自行结束 (除非停止循环)
        cur = cur + 1;
    } else if (mode == 1) {
        // 递减 A-1→B: cur > B 则继续
        if (cur > B) { cur = cur - 1; } else { finished = true; }
    } else {
        // 递增 A→B-1: cur < B-1 则继续
        if (cur < B - 1) { cur = cur + 1; } else { finished = true; }
    }

    if (finished) {
        context.setData("__loop_active", false);
        setResultData("loopIndex", cur);
        setResultData("loopComplete", true);
        setResultData("loopBroken", false);
        setStatus(ToolStatus::OK);
        return true;
    }

    // 继续循环: 写回索引, 请求回跳
    context.setData(indexKey, cur);
    context.setMessage(1);
    setResultData("loopIndex", cur);
    setResultData("loopComplete", false);
    setResultData("loopBroken", false);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(LoopEnd, "循环结束", VisionInspector::ToolCategory::Logic)
