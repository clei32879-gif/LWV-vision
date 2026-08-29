#include "CalculateVariable.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>
#include <QStack>

namespace VisionInspector {

PropertyDefList CalculateVariable::propertyDefs() const {
    return {
        PropertyDef::enumProp("outputType", "输出类型", {"浮点数", "整数"}, 0),
        PropertyDef::stringProp("varName", "变量名", "result"),
        PropertyDef::stringProp("expression", "表达式", "0"),
        PropertyDef::stringProp("link1", "数据链接1", ""),
        PropertyDef::stringProp("link2", "数据链接2", ""),
        PropertyDef::stringProp("link3", "数据链接3", ""),
    };
}

// 简易表达式解析器
static double evalExpression(const QString& expr, const QMap<QString, double>& vars) {
    QString e = expr.trimmed();
    // 替换变量
    for (auto it = vars.begin(); it != vars.end(); ++it) {
        e.replace(it.key(), QString::number(it.value(), 'g', 15));
    }
    QStack<double> nums;
    QStack<char> ops;
    auto applyOp = [&]() {
        if (ops.isEmpty() || nums.size() < 2) return;
        char op = ops.pop();
        double b = nums.pop();
        double a = nums.pop();
        switch (op) {
        case '+': nums.push(a + b); break;
        case '-': nums.push(a - b); break;
        case '*': nums.push(a * b); break;
        case '/': nums.push(b != 0 ? a / b : 0); break;
        case '^': nums.push(std::pow(a, b)); break;
        }
    };
    auto precedence = [](char c) -> int {
        if (c == '+' || c == '-') return 1;
        if (c == '*' || c == '/') return 2;
        if (c == '^') return 3;
        return 0;
    };
    int i = 0;
    while (i < e.length()) {
        QChar c = e[i];
        if (c.isSpace()) { i++; continue; }
        if (c == '(') { ops.push('('); i++; }
        else if (c == ')') { while (!ops.isEmpty() && ops.top() != '(') applyOp(); if (!ops.isEmpty()) ops.pop(); i++; }
        else if (c.isDigit() || c == '.' || (c == '-' && (i == 0 || e[i-1] == '(' || e[i-1] == '+' || e[i-1] == '-' || e[i-1] == '*' || e[i-1] == '/' || e[i-1] == '^'))) {
            int start = i;
            if (c == '-') i++;
            while (i < e.length() && (e[i].isDigit() || e[i] == '.')) i++;
            nums.push(e.mid(start, i - start).toDouble());
        } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
            while (!ops.isEmpty() && ops.top() != '(' && precedence(ops.top()) >= precedence(c.toLatin1()))
                applyOp();
            ops.push(c.toLatin1());
            i++;
        } else { i++; }
    }
    while (!ops.isEmpty()) applyOp();
    return nums.isEmpty() ? 0 : nums.top();
}

bool CalculateVariable::execute(ToolContext& context) {
    QString expr = propertyValue("expression").toString();
    QString varName = propertyValue("varName").toString();
    QMap<QString, double> vars;
    for (int i = 1; i <= 3; ++i) {
        QString key = propertyValue(QString("link%1").arg(i)).toString();
        if (!key.isEmpty() && context.hasData(key)) {
            vars[QString("$%1").arg(i)] = context.getDouble(key);
        }
    }
    double result = evalExpression(expr, vars);
    int outputType = propertyValue("outputType").toInt();
    if (outputType == 0) {
        context.setData(varName, result);
        setResultData(varName, result);
    } else {
        int intResult = (int)result;
        context.setData(varName, intResult);
        setResultData(varName, intResult);
    }
    setResultData("expression", expr);
    setResultData("computed", true);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CalculateVariable, "计算变量", VisionInspector::ToolCategory::Logic)
