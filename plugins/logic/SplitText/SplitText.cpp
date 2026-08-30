/**
 * @file SplitText.cpp
 * @brief 分解文本工具 (对标 CKVision 分解文本, §2.7 补齐)
 *
 * 按分隔符把文本拆成多段, 供后续取值/拼装使用。
 * 输入文本可用 {1} 占位引用"生成文本"等上游输出, 或直接填属性 inputText。
 * 常用场景: 解析通信报文 "A,B,C" -> part0=A part1=B part2=C
 *
 * 输出: partCount / partN(各段) / selected(指定index段, -1返回全部)
 */
#include "SplitText.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QStringList>

namespace VisionInspector {

PropertyDefList SplitText::propertyDefs() const {
    return {
        PropertyDef::stringProp("inputText", "输入文本", ""),
        PropertyDef::enumProp("delimiter", "分隔符",
                              {"逗号,", "分号;", "空格", "竖线|", "自定义"}, 0),
        PropertyDef::stringProp("customDelimiter", "自定义分隔符", ","),
        PropertyDef::intProp("selectIndex", "取第N段(-1全部)", -1, -1, 10000),
    };
}

bool SplitText::execute(ToolContext& context) {
    QString text = propertyValue("inputText").toString();
    if (text.isEmpty() && context.hasData("text"))
        text = context.getData("text").toString();

    const int delim = propertyValue("delimiter").toInt();
    QString sep;
    switch (delim) {
    case 0: sep = ","; break;
    case 1: sep = ";"; break;
    case 2: sep = " "; break;
    case 3: sep = "|"; break;
    default: sep = propertyValue("customDelimiter").toString(); break;
    }

    QStringList parts = text.split(sep, Qt::KeepEmptyParts);
    setResultData("partCount", (int)parts.size());
    for (int i = 0; i < parts.size(); ++i)
        setResultData(QStringLiteral("part%1").arg(i), parts.at(i));

    const int sel = propertyValue("selectIndex").toInt();
    if (sel >= 0 && sel < parts.size())
        setResultData("selected", parts.at(sel));
    else if (sel >= parts.size())
        setResultData("selected", QString());
    else
        setResultData("selected", parts.join(sep));

    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(SplitText, "分解文本", VisionInspector::ToolCategory::Logic)
