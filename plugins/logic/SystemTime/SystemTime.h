/** @file SystemTime.h - 系统时间/耗时计时工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class SystemTime : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("SystemTime"); }
    QString displayName() const override { return QStringLiteral("系统时间"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;

private:
    // 耗时模式: 从首个周期起算, 记录首帧时刻与上一帧时刻 (H-4 线程安全)
    mutable std::mutex m_timerMutex;
    qint64 m_firstUs = 0;
    qint64 m_lastUs = 0;
    int m_runCount = 0;
};
} // namespace VisionInspector
