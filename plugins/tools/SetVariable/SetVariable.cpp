#include "SetVariable.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/GlobalVariables.h"

namespace VisionInspector {

PropertyDefList SetVariable::propertyDefs() const {
    return {
        PropertyDef::enumProp("varType", "变量类型", {"浮点数", "整数", "布尔", "字符串"}, 0),
        PropertyDef::stringProp("varName", "变量名", "myVar"),
        PropertyDef::doubleProp("doubleValue", "浮点数值", 0.0, -1e9, 1e9),
        PropertyDef::intProp("intValue", "整数值", 0, -2147483647, 2147483647),
        PropertyDef::boolProp("boolValue", "布尔值", false),
        PropertyDef::stringProp("stringValue", "字符串值", ""),
        PropertyDef::stringProp("linkVar", "数据链接", ""),
        PropertyDef::enumProp("scope", "写入范围", {"当前流程", "全局变量"}, 0),
    };
}

bool SetVariable::execute(ToolContext& context) {
    QString varName = propertyValue("varName").toString();
    int varType = propertyValue("varType").toInt();
    const int scope = propertyValue("scope").toInt();   // 0当前流程 1全局变量

    // 计算要写入的值
    QVariant val;
    if (!propertyValue("linkVar").toString().isEmpty() && context.hasData(propertyValue("linkVar").toString())) {
        val = context.getData(propertyValue("linkVar").toString());
    } else {
        switch (varType) {
        case 0:  val = propertyValue("doubleValue").toDouble(); break;
        case 1:  val = propertyValue("intValue").toInt(); break;
        case 2:  val = propertyValue("boolValue").toBool(); break;
        default: val = propertyValue("stringValue").toString(); break;
        }
    }

    // 按范围写入
    if (scope == 1) {
        if (!context.globalVariables()) {
            setResultData("error", "全局变量管理器未初始化");
            setStatus(ToolStatus::NG);
            return false;
        }
        context.globalVariables()->set(varName, val);
    } else {
        context.setData(varName, val);
    }
    setResultData(varName, val);
    setResultData("scope", scope);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(SetVariable, "设置变量", VisionInspector::ToolCategory::Logic)
