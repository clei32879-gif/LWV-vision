/** @file ScriptNode.cpp - 脚本节点实现 (QJSEngine) */
#include "ScriptNode.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/GlobalVariables.h"
#include "../../../src/utils/Logger.h"
#ifdef VI_HAS_QJS
#include <QJSEngine>
#include <QJSValueIterator>
#endif

namespace VisionInspector {

PropertyDefList ScriptNode::propertyDefs() const {
    return {
        PropertyDef::stringProp("script", "JS脚本",
            "// 例: 缺陷数>0 且 置信度达标 则 NG\n"
            "var cnt = data[\"检测宽窄.width\"];\n"
            "log(\"宽度=\" + cnt);\n"
            "true;", "脚本"),
        PropertyDef::intProp("timeoutMs", "执行超时(ms, 预留)", 500, 50, 10000, "安全"),
    };
}

bool ScriptNode::execute(ToolContext& context) {
#ifndef VI_HAS_QJS
    setResultData("error", "Qt Qml 模块未启用, 脚本节点不可用");
    setStatus(ToolStatus::NG); return false;
#else
    QJSEngine engine;

    // ---- data 对象: 上下文数据快照 (读写同步回 context) ----
    QJSValue dataObj = engine.newObject();
    const DataMap snapshot = context.allData();
    for (auto it = snapshot.begin(); it != snapshot.end(); ++it)
        dataObj.setProperty(it.key(), engine.toScriptValue(QVariant(it.value()).toString()));
    // 便捷: 属性读写走 QJSValue 代理太重, 简化为执行前快照 + 执行后回收脚本新建键
    engine.globalObject().setProperty(QStringLiteral("data"), dataObj);

    // ---- globals 对象: 全局变量 ----
    QJSValue globObj = engine.newObject();
    if (context.globalVariables()) {
        const auto gv = context.globalVariables()->all();
        for (auto it = gv.begin(); it != gv.end(); ++it)
            globObj.setProperty(it.key(), engine.toScriptValue(it.value()));
    }
    engine.globalObject().setProperty(QStringLiteral("globals"), globObj);

    // ---- input: 当前图像信息 (只读) ----
    QJSValue inputObj = engine.newObject();
    if (auto img = context.currentImage()) {
        inputObj.setProperty(QStringLiteral("width"), img->cols);
        inputObj.setProperty(QStringLiteral("height"), img->rows);
        inputObj.setProperty(QStringLiteral("channels"), img->channels());
    }
    engine.globalObject().setProperty(QStringLiteral("input"), inputObj);

    // ---- log 函数 ----
    QJSValue logFn = engine.evaluate(
        QStringLiteral("(function(msg) { __script_logs.push(String(msg)); })"));
    QJSValue logs = engine.newArray();
    engine.globalObject().setProperty(QStringLiteral("__script_logs"), logs);
    // 绑定真实推送: 简化 — 收集后统一回读
    engine.globalObject().setProperty(QStringLiteral("log"), logFn);

    const QString script = propertyValue("script").toString();
    QJSValue result = engine.evaluate(script, QStringLiteral("script.js"));
    if (result.isError()) {
        setResultData("error", QStringLiteral("脚本错误: %1 (行%2)")
            .arg(result.toString(),
                 result.property(QStringLiteral("lineNumber")).toString()));
        setStatus(ToolStatus::NG);
        return false;
    }

    // ---- 回收: log 输出 ----
    const QJSValue logsAfter = engine.globalObject().property(QStringLiteral("__script_logs"));
    const int logLen = logsAfter.property(QStringLiteral("length")).toInt();
    QStringList logLines;
    for (int i = 0; i < qMin(logLen, 50); ++i)
        logLines << logsAfter.property(QString::number(i)).toString();
    if (!logLines.isEmpty())
        setResultData("log", logLines.join(QStringLiteral("\n")));

    // ---- 回收: data 对象写回上下文 (脚本可产出数据键) ----
    QJSValueIterator dataIt(dataObj);
    int written = 0;
    while (dataIt.hasNext()) {
        dataIt.next();
        const QString key = dataIt.name();
        const QJSValue v = dataIt.value();
        if (v.isBool())        context.setData(key, v.toBool());
        else if (v.isNumber()) context.setData(key, v.toNumber());
        else if (v.isString()) context.setData(key, v.toString());
        else continue;
        ++written;
    }
    // ---- 回收: globals 修改 ----
    if (context.globalVariables()) {
        QJSValueIterator gIt(globObj);
        while (gIt.hasNext()) {
            gIt.next();
            const QJSValue v = gIt.value();
            if (v.isBool())      context.globalVariables()->set(gIt.name(), v.toBool());
            else if (v.isNumber()) context.globalVariables()->set(gIt.name(), v.toNumber());
            else if (v.isString()) context.globalVariables()->set(gIt.name(), v.toString());
        }
    }

    setResultData("writtenKeys", written);
    const bool ok = result.isBool() ? result.toBool() : result.toBool();
    setResultData("ok", ok);
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ScriptNode, "脚本节点", VisionInspector::ToolCategory::Logic)
