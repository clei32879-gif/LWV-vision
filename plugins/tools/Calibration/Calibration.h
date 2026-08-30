/** @file Calibration.h - 标定校准工具（像素→毫米转换 + 相机内参/畸变标定） */
#pragma once
#include "../../../src/engine/ITool.h"
namespace VisionInspector {
class Calibration : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("Calibration"); }
    QString displayName() const override { return QStringLiteral("标定校准"); }
    ToolCategory category() const override { return ToolCategory::Calibration; }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;

private:
    // calibMethod=3: 从目录读取多张不同姿态棋盘格图, calibrateCamera 求内参/畸变
    bool calibrateCameraMulti(ToolContext& context);
};
} // namespace VisionInspector
