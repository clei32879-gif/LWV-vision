/**
 * @file Loop.cpp
 * @brief 循环工具-开始循环 (对标 CKVision 循环工具, M-23 修复)
 *
 * 与"结束循环"(LoopEnd) 配对, 重复执行两者之间的流程段落。
 * 引擎根据 message==1 回跳到 __loop_start 索引; 循环状态存于上下文。
 *
 * 循环模式:
 *   0 递增  从 A 到 B-1, 循环次数 B-A, 索引每次 +1
 *   1 递减  从 A-1 到 B, 循环次数 A-B, 索引每次 -1
 *   2 无限  无限循环, 直到"停止循环"或停止运行
 *
 * 索引值写入 indexKey (默认 loopIndex), 供循环体内工具读取。
 * 输出: loopIndex / loopActive / loopStart / loopEnd / loopCount
 */
#include "Loop.h"
#include "../../../src/engine/ToolRegistry.h"
#include <algorithm>

namespace VisionInspector {

PropertyDefList Loop::propertyDefs() const {
    return {
        PropertyDef::enumProp("loopMode", "循环模式",
                              {"递增 A→B-1", "递减 A-1→B", "无限"}, 0),
        PropertyDef::intProp("startValue", "起始值A", 1, -100000, 100000),
        PropertyDef::intProp("endValue", "结束值B", 5, -100000, 100000),
        PropertyDef::stringProp("indexKey", "循环索引变量名", "loopIndex"),
    };
}

bool Loop::execute(ToolContext& context) {
    const int mode = propertyValue("loopMode").toInt();
    const int A = propertyValue("startValue").toInt();
    const int B = propertyValue("endValue").toInt();
    const QString indexKey = propertyValue("indexKey").toString();
    const QString key = indexKey.isEmpty() ? QStringLiteral("loopIndex") : indexKey;

    // 记录循环起点索引 (引擎在执行前写入 __engine_index)
    const int selfIdx = context.getInt("__engine_index", -1);
    if (selfIdx >= 0)
        context.setData("__loop_start", selfIdx);

    // 循环元数据
    context.setData("__loop_mode", mode);
    context.setData("__loop_A", A);
    context.setData("__loop_B", B);
    context.setData("__loop_index_key", key);

    // 首次进入: 初始化索引并激活循环
    if (!context.getBool("__loop_active", false)) {
        context.setData("__loop_active", true);
        context.setData("__loop_break", false);
        int cur = (mode == 1) ? A - 1 : A;   // 递减从 A-1 开始
        context.setData(key, cur);
    }

    const int cur = context.getInt(key, A);
    setResultData("loopIndex", cur);
    setResultData("loopActive", true);
    setResultData("loopStart", selfIdx);
    setResultData("loopCount", std::max(0, B - A));
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(Loop, "循环", VisionInspector::ToolCategory::Logic)
