/**
 * @file SystemTime.cpp
 * @brief 系统时间/耗时计时工具 (对标 CKVision 计算时间/系统时间)
 *
 * 模式0 时间戳: 输出当前系统时间 (日期时间字符串 + Unix毫秒)
 * 模式1 耗时计时: 输出本工具自首次执行起的运行耗时 (总耗时/单次间隔),
 *                每次执行都刷新基准 (用于测量单流程周期、等待节拍)
 *
 * 输出:
 *   datetime      "yyyy-MM-dd HH:mm:ss.zzz"
 *   unixMs        当前 Unix 毫秒
 *   mode          0时间戳 / 1耗时
 * 耗时模式额外:
 *   elapsedMs     距首次执行毫秒
 *   lastIntervalMs 距上一次执行间隔
 *   runCount      累计执行次数
 */
#include "SystemTime.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QDateTime>

namespace VisionInspector {

PropertyDefList SystemTime::propertyDefs() const {
    return {
        PropertyDef::enumProp("mode", "模式", {"时间戳", "耗时计时"}, 0),
    };
}

bool SystemTime::execute(ToolContext& /*context*/) {
    const int mode = propertyValue("mode").toInt();
    const qint64 nowUs = QDateTime::currentMSecsSinceEpoch() * 1000;

    if (mode == 0) {
        // 时间戳模式
        const QDateTime dt = QDateTime::currentDateTime();
        setResultData("datetime", dt.toString("yyyy-MM-dd HH:mm:ss.zzz"));
        setResultData("unixMs", nowUs / 1000);
        setResultData("mode", 0);
        setStatus(ToolStatus::OK);
        return true;
    }

    // 耗时计时模式
    std::lock_guard<std::mutex> lk(m_timerMutex);
    if (m_firstUs == 0)
        m_firstUs = nowUs;
    const qint64 elapsedUs = nowUs - m_firstUs;
    const qint64 intervalUs = (m_lastUs == 0) ? 0 : (nowUs - m_lastUs);
    m_lastUs = nowUs;
    ++m_runCount;

    setResultData("mode", 1);
    setResultData("elapsedMs", elapsedUs / 1000.0);
    setResultData("lastIntervalMs", intervalUs / 1000.0);
    setResultData("runCount", (int)m_runCount);
    setResultData("unixMs", nowUs / 1000);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(SystemTime, "系统时间", VisionInspector::ToolCategory::Logic)
