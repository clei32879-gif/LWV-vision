/** @file SelectBranch.h - 选择分支工具 (对标 CKVision 选择分支, P0) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {

/**
 * 选择分支: 按 branchIndex 选择执行 N 路分支之一。
 * 结构化约定 (与 Loop/LoopEnd 同款消息跳转机制, 无需改引擎):
 *   流程: [选择分支] → case0 工具... → [分支结束(branchId=0)] → case1 工具... → [分支结束(branchId=1)] ...
 *   - 选择分支: 记录 "__branch_after_<selfIdx>" = selfIdx+1 (所有分支跑完后回到这里)
 *               找到 case k 分支的首工具索引, setMessage(3) + __next_index 跳过去
 *   - 分支结束: 若自身 branchId == 当前执行分支k → 跳回 __branch_after_<selfIdx> (setMessage(3))
 *               否则顺序继续 (经过其他分支体)
 */
class SelectBranch : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("SelectBranch"); }
    QString displayName() const override { return QStringLiteral("选择分支"); }
    ToolCategory category() const override { return ToolCategory::Logic; }
    QString description() const override { return QStringLiteral(
        "按选择索引跳到对应分支执行(case 0~7), 分支末尾配分支结束工具；用于多方案切换"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
