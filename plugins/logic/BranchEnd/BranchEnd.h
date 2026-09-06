/** @file BranchEnd.h - 分支结束工具 (与选择分支配对, P0) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

/** 分支结束: 本分支体执行完毕后跳回选择分支之后的公共路径 */
class BranchEnd : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("BranchEnd"); }
    QString displayName() const override { return QStringLiteral("分支结束"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral(
        "每路分支末尾放置并填写所属分支号, 执行完该分支即跳回公共路径；与选择分支配对"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
