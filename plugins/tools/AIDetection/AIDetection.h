/** @file AIDetection.h - AI检测工具节点 (ONNX模型推理, 阶段4) */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class AIDetection : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("AIDetection"); }
    QString displayName() const override { return QStringLiteral("AI检测"); }
    ToolCategory category() const override { return ToolCategory::Special; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;
private:
    // 叠加缓存
    struct Box { float x, y, w, h; int cls; float score; };
    std::vector<Box> m_boxes;
    bool m_lastOk = false;
};
} // namespace VisionInspector
