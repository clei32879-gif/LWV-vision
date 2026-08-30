/**
 * @file StopLoop.cpp
 * @brief 停止循环工具 (对标 CKVision 停止循环, P0-9 补齐)
 *
 * 放在"循环"与"循环结束"之间, 条件满足时立即退出循环:
 * - 无条件: 直接停止循环
 * - 流程NG: 当流程出现 NG (上下文 __flow_ng 置位) 时停止
 * - 数据满足: 读 sourceDataKey 值, 满足表达式(如 "> 2")时停止
 *
 * 触发后置 __loop_break 并发送 message=2, 引擎跳到循环结束之后;
 * LoopEnd 也会检查 __loop_break 确保循环状态被清除。
 * 输出: stopped(bool) / reason
 */
#include "StopLoop.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList StopLoop::propertyDefs() const {
    return {
        PropertyDef::enumProp("stopWhen", "停止条件",
                              {"无条件", "流程NG", "数据满足"}, 0),
        PropertyDef::stringProp("sourceDataKey", "来源数据Key", ""),
        PropertyDef::stringProp("expression", "条件表达式 (如: > 2)", ""),
    };
}

// 简单表达式求值 (数值比较): 支持 > < >= <= == !=, 单条件
static bool evalSimpleCondition(const QString& expr, double value) {
    QString e = expr.trimmed();
    if (e.isEmpty()) return false;

    int opPos = -1, opLen = 0;
    QString op;
    static const char* ops[] = {">=", "<=", "!=", "==", ">", "<"};
    for (const char* o : ops) {
        int pos = e.indexOf(QString::fromLatin1(o));
        if (pos >= 0) { op = QString::fromLatin1(o); opPos = pos; opLen = (int)op.size(); break; }
    }
    if (opPos < 0) return false;

    const QString rhsStr = e.mid(opPos + opLen).trimmed();
    bool ok = false;
    const double rhs = rhsStr.toDouble(&ok);
    if (!ok) return false;

    if (op == ">")   return value > rhs;
    if (op == "<")   return value < rhs;
    if (op == ">=")  return value >= rhs;
    if (op == "<=")  return value <= rhs;
    if (op == "==")  return std::fabs(value - rhs) < 1e-9;
    if (op == "!=")  return std::fabs(value - rhs) >= 1e-9;
    return false;
}

bool StopLoop::execute(ToolContext& context) {
    const int stopWhen = propertyValue("stopWhen").toInt();
    bool stop = false;
    QString reason;

    switch (stopWhen) {
    case 0:  // 无条件
        stop = true;
        reason = "无条件";
        break;
    case 1:  // 流程NG
        stop = context.getBool("__flow_ng", false);
        reason = stop ? "流程NG" : "流程OK";
        break;
    default: {  // 数据满足
        const QString key = propertyValue("sourceDataKey").toString();
        const QString expr = propertyValue("expression").toString();
        QVariant v;
        if (!key.isEmpty() && context.hasData(key))
            v = context.getData(key);
        const double value = v.toDouble();
        stop = evalSimpleCondition(expr, value);
        reason = QStringLiteral("数据满足: 值%1 %2").arg(value).arg(expr);
        break;
    }
    }

    if (stop) {
        context.setData("__loop_break", true);
        context.setData("__loop_active", false);   // 停止时即清除活动标志, 避免残留
        context.setMessage(2);   // 引擎跳到 __loop_end 之后
        setResultData("stopped", true);
        setResultData("reason", reason);
    } else {
        setResultData("stopped", false);
        setResultData("reason", reason);
    }
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(StopLoop, "停止循环", VisionInspector::ToolCategory::Logic)
