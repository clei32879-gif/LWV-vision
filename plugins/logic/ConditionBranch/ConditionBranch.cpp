#include "ConditionBranch.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QStringList>
#include <cmath>

namespace VisionInspector {

PropertyDefList ConditionBranch::propertyDefs() const {
    return {
        PropertyDef::stringProp("sourceToolName", "来源工具名", ""),
        PropertyDef::stringProp("sourceDataKey", "来源数据Key", ""),
        PropertyDef::stringProp("expression", "条件表达式 (如: score > 0.95)", ""),
        PropertyDef::enumProp("conditionLogic", "条件逻辑", {"AND", "OR"}, 0),
    };
}

static bool evaluateSingleCondition(const QString& condition, double value) {
    QString expr = condition.trimmed();
    if (expr.isEmpty()) return true;

    int opPos = -1;
    int opLen = 0;
    QString op;
    for (int i = 0; i < expr.length(); ++i) {
        if (i + 1 < expr.length()) {
            QString two = expr.mid(i, 2);
            if (two == ">=") { op = ">="; opPos = i; opLen = 2; break; }
            if (two == "<=") { op = "<="; opPos = i; opLen = 2; break; }
            if (two == "!=") { op = "!="; opPos = i; opLen = 2; break; }
            if (two == "==") { op = "=="; opPos = i; opLen = 2; break; }
        }
        if (expr[i] == '>') { op = ">"; opPos = i; opLen = 1; break; }
        if (expr[i] == '<') { op = "<"; opPos = i; opLen = 1; break; }
    }
    if (opPos < 0) return false;

    QString rhsStr = expr.mid(opPos + opLen).trimmed();
    bool ok = false;
    double rhs = rhsStr.toDouble(&ok);
    if (!ok) return false;

    if (op == ">")   return value > rhs;
    if (op == "<")   return value < rhs;
    if (op == ">=")  return value >= rhs;
    if (op == "<=")  return value <= rhs;
    // M-24修复: qFuzzyCompare(a,0)恒false导致"==0"判断错误 → 改用容差比较
    if (op == "==")  return std::fabs(value - rhs) < 1e-9;
    if (op == "!=")  return std::fabs(value - rhs) >= 1e-9;
    return false;
}

bool ConditionBranch::execute(ToolContext& context) {
    QString sourceTool = propertyValue("sourceToolName").toString();
    QString dataKey = propertyValue("sourceDataKey").toString();
    QString expression = propertyValue("expression").toString();
    int logicOp = propertyValue("conditionLogic").toInt();

    if (sourceTool.isEmpty() || expression.isEmpty()) {
        setResultData("pass", false);
        setResultData("error", "来源工具名或表达式为空");
        setStatus(ToolStatus::NG);
        return false;
    }

    QVariant value;
    if (!dataKey.isEmpty()) {
        value = context.getData(sourceTool + "." + dataKey);
    } else {
        if (context.hasData(sourceTool)) {
            value = context.getData(sourceTool);
        }
    }

    setResultData("sourceValue", value);

    QStringList condList = expression.split(";", Qt::SkipEmptyParts);
    if (condList.isEmpty()) {
        setResultData("pass", true);
        setStatus(ToolStatus::OK);
        return true;
    }

    double numVal = value.toDouble();
    bool allPass = (logicOp == 0);
    QStringList detailList;

    for (const QString& cond : condList) {
        QString trimmed = cond.trimmed();
        if (trimmed.isEmpty()) continue;

        bool ok = evaluateSingleCondition(trimmed, numVal);
        detailList << (ok ? "true" : "false");

        if (logicOp == 0) allPass = allPass && ok;
        else allPass = allPass || ok;
    }

    setResultData("pass", allPass);
    setResultData("detail", detailList.join(","));

    if (allPass) {
        setResultData("branch", "pass");
        setStatus(ToolStatus::OK);
    } else {
        setResultData("branch", "fail");
        setStatus(ToolStatus::NG);
    }
    return true;
}

VI_REGISTER_TOOL(ConditionBranch, "条件分支", VisionInspector::ToolCategory::Logic)
} // namespace VisionInspector
