/**
 * @file GenerateText.cpp
 * @brief 生成文本工具 (对标 CKVision 生成文本, §2.7 补齐)
 *
 * 用模板字符串 + 变量值生成文本, 用于拼装结果报文/标签/日志。
 * 模板中的 {key} 占位符从三处取值(优先级从高到低):
 *   1) 直接输入属性 var1..var5 (模板写 {1}..{5})
 *   2) 流程上下文数据 (context.getData, 含其他工具写入的 "工具名.键")
 *   3) 全局变量 (GlobalVariables)
 * 示例: 模板 "X={1}  Y={circle.radius}" -> 输出 "X=12.3  Y=45.6"
 *
 * 输出: text(生成文本) / replacedCount(替换次数)
 */
#include "GenerateText.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/GlobalVariables.h"
#include <QRegularExpression>

namespace VisionInspector {

PropertyDefList GenerateText::propertyDefs() const {
    return {
        PropertyDef::stringProp("template", "模板文本", ""),
        PropertyDef::stringProp("var1", "变量1", ""),
        PropertyDef::stringProp("var2", "变量2", ""),
        PropertyDef::stringProp("var3", "变量3", ""),
        PropertyDef::stringProp("var4", "变量4", ""),
        PropertyDef::stringProp("var5", "变量5", ""),
    };
}

bool GenerateText::execute(ToolContext& context) {
    const QString tpl = propertyValue("template").toString();
    QString out = tpl;
    int replaced = 0;

    static const QRegularExpression re(R"(\{([^}]+)\})");
    QRegularExpressionMatchIterator it = re.globalMatch(tpl);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString key = m.captured(1).trimmed();
        QVariant v;

        // 1) 直接输入变量 {1}..{5}
        if (key.size() == 1 && key[0] >= '1' && key[0] <= '5') {
            v = propertyValue(QStringLiteral("var%1").arg(key));
        }
        // 2) 流程上下文
        if (!v.isValid() && context.hasData(key))
            v = context.getData(key);
        // 3) 全局变量
        if (!v.isValid() && context.globalVariables())
            v = context.globalVariables()->get(key);

        if (v.isValid()) {
            out.replace(m.captured(0), v.toString());
            ++replaced;
        }
    }

    setResultData("text", out);
    setResultData("replacedCount", replaced);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(GenerateText, "生成文本", VisionInspector::ToolCategory::Logic)
