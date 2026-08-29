#include "SetVariable.h"
#include "../../../src/engine/ToolRegistry.h"

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
    };
}

bool SetVariable::execute(ToolContext& context) {
    QString varName = propertyValue("varName").toString();
    int varType = propertyValue("varType").toInt();
    // 检查数据链接
    QString linkVar = propertyValue("linkVar").toString();
    if (!linkVar.isEmpty() && context.hasData(linkVar)) {
        QVariant val = context.getData(linkVar);
        context.setData(varName, val);
        setResultData(varName, val);
        setStatus(ToolStatus::OK);
        return true;
    }
    // 使用直接设置的值
    switch (varType) {
    case 0: {
        double val = propertyValue("doubleValue").toDouble();
        context.setData(varName, val);
        setResultData(varName, val);
        break;
    }
    case 1: {
        int val = propertyValue("intValue").toInt();
        context.setData(varName, val);
        setResultData(varName, val);
        break;
    }
    case 2: {
        bool val = propertyValue("boolValue").toBool();
        context.setData(varName, val);
        setResultData(varName, val);
        break;
    }
    case 3: {
        QString val = propertyValue("stringValue").toString();
        context.setData(varName, val);
        setResultData(varName, val);
        break;
    }
    }
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(SetVariable, "设置变量", VisionInspector::ToolCategory::Logic)
