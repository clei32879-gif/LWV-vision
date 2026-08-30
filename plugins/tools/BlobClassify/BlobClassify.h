/** @file BlobClassify.h - 斑点分类工具 (对标 CKVision 斑点分类, P1-13 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
#include "../../../src/engine/ROI.h"
namespace VisionInspector {
class BlobClassify : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("BlobClassify"); }
    QString displayName() const override { return QStringLiteral("斑点分类"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override { return QStringLiteral("BLOB特征分类: 面积/周长/圆度/长宽比/孔数 + 阈值判定"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
private:
    ROIRegion m_roi;
};
} // namespace VisionInspector
