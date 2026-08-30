/**
 * @file CompareText.cpp
 * @brief 比较文本工具 (对标 CKVision 比较文本, §2.7 补齐)
 *
 * 比较两段文本, 用于报文校验/结果判定。
 * textA 与 textB 均可直接填属性, 或用 {1}/{2} 引用上游输出(执行前由引擎解析)。
 *
 * 比较模式:
 *   0 等于  1 不等于  2 包含  3 开头  4 结尾  5 正则匹配
 *
 * 输出: equal(bool) / matchDetail / matched(bool, 兼容)
 * 不匹配时状态置 NG (可挂上下限判定/结束补正联动)。
 */
#include "CompareText.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QRegularExpression>
#include <Qt>

namespace VisionInspector {

PropertyDefList CompareText::propertyDefs() const {
    return {
        PropertyDef::stringProp("textA", "文本A", ""),
        PropertyDef::stringProp("textB", "文本B", ""),
        PropertyDef::enumProp("compareMode", "比较方式",
                              {"等于", "不等于", "包含", "开头", "结尾", "正则匹配"}, 0),
        PropertyDef::boolProp("caseSensitive", "区分大小写", true),
    };
}

bool CompareText::execute(ToolContext& context) {
    QString a = propertyValue("textA").toString();
    QString b = propertyValue("textB").toString();
    const int mode = propertyValue("compareMode").toInt();
    const bool cs = propertyValue("caseSensitive").toBool();
    const Qt::CaseSensitivity sensitivity = cs ? Qt::CaseSensitive : Qt::CaseInsensitive;

    bool ok = false;
    QString detail;
    switch (mode) {
    case 0: ok = QString::compare(a, b, sensitivity) == 0; detail = "等于"; break;
    case 1: ok = QString::compare(a, b, sensitivity) != 0; detail = "不等于"; break;
    case 2: ok = a.contains(b, sensitivity);              detail = "包含"; break;
    case 3: ok = a.startsWith(b, sensitivity);            detail = "开头"; break;
    case 4: ok = a.endsWith(b, sensitivity);              detail = "结尾"; break;
    case 5: {
        const QRegularExpression re(b, cs ? QRegularExpression::NoPatternOption
                                         : QRegularExpression::CaseInsensitiveOption);
        ok = re.isValid() && re.match(a).hasMatch();
        detail = re.isValid() ? "正则匹配" : "正则无效";
        break;
    }
    default: break;
    }

    setResultData("equal", ok);
    setResultData("matched", ok);
    setResultData("matchDetail", detail);
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CompareText, "比较文本", VisionInspector::ToolCategory::Logic)
