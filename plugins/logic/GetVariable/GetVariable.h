/** @file GetVariable.h - 读取全局变量工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class GetVariable : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("GetVariable"); }
    QString displayName() const override { return QStringLiteral("获取全局变量"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
