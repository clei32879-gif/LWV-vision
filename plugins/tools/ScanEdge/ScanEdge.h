/** @file ScanEdge.h - 扫描边缘工具 (对标 CKVision 扫描边缘, P1-12 补齐) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class ScanEdge : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("ScanEdge"); }
    QString displayName() const override { return QStringLiteral("扫描边缘"); }
    ToolCategory category() const override { return ToolCategory::Geometry; }
    QString description() const override { return QStringLiteral("沿扫描线检测全部边缘点坐标"); }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
