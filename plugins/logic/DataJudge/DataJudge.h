/** @file DataJudge.h - DataJudge逻辑工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class DataJudge : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("数据判断"); }
    QString displayName() const override { return QStringLiteral("数据判断"); }
    QString description() const override { return QStringLiteral("根据阈值判断数据OK/NG并统计"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
