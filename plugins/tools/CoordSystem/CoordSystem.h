/** @file CoordSystem.h - 坐标系统工具 */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class CoordSystem : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("CoordSystem"); }
    QString displayName() const override { return QStringLiteral("坐标系统"); }
    ToolCategory category() const override { return ToolCategory::Calibration; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
};
} // namespace VisionInspector
