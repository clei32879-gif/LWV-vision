/**
 * @file GetVariable.cpp
 * @brief 获取全局变量工具 (对标 CKVision 获取全局变量, §2.7 补齐)
 *
 * 从全局变量管理器读取指定变量值 (跨流程共享, 线程安全)。
 * 可与 设置变量 的"写入全局变量"配合, 或读取其他工具 applyTo=所有流程 写入的标定结果。
 *
 * 输入: varName 变量名
 * 输出: value/typeName/found
 */
#include "GetVariable.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/GlobalVariables.h"

namespace VisionInspector {

PropertyDefList GetVariable::propertyDefs() const {
    return {
        PropertyDef::stringProp("varName", "变量名", "myVar"),
    };
}

bool GetVariable::execute(ToolContext& context) {
    const QString varName = propertyValue("varName").toString();
    if (varName.isEmpty()) {
        setResultData("error", "变量名为空");
        setStatus(ToolStatus::NG);
        return false;
    }
    if (!context.globalVariables()) {
        setResultData("error", "全局变量管理器未初始化");
        setStatus(ToolStatus::NG);
        return false;
    }
    const QVariant v = context.globalVariables()->get(varName);
    setResultData("value", v);
    setResultData("typeName", v.typeName());
    setResultData("found", v.isValid());
    setResultData("varName", varName);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(GetVariable, "获取全局变量", VisionInspector::ToolCategory::Logic)
