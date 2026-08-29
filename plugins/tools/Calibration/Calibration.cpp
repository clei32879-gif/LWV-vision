#include "Calibration.h"
#include "../../../src/engine/ToolRegistry.h"

namespace VisionInspector {

PropertyDefList Calibration::propertyDefs() const {
    return {
        PropertyDef::enumProp("calibMethod", "标定方法", {"两点标定", "已知比例", "棋盘格标定"}, 1),
        PropertyDef::doubleProp("pixelLength", "像素长度", 100.0, 1, 10000),
        PropertyDef::doubleProp("realLength", "实际长度(mm)", 10.0, 0.001, 10000),
        PropertyDef::doubleProp("pixelRatio", "像素比例(mm/pixel)", 0.1, 0.0001, 100),
        PropertyDef::stringProp("unit", "单位", "mm"),
        PropertyDef::enumProp("applyTo", "应用范围", {"当前流程", "所有流程"}, 0),
    };
}

bool Calibration::execute(ToolContext& context) {
    int method = propertyValue("calibMethod").toInt();
    double ratio = 0;

    if (method == 0) {
        // 两点标定
        double pixelLen = propertyValue("pixelLength").toDouble();
        double realLen = propertyValue("realLength").toDouble();
        if (pixelLen > 0) {
            ratio = realLen / pixelLen;
        }
    } else if (method == 1) {
        // 已知比例
        ratio = propertyValue("pixelRatio").toDouble();
    } else {
        // 棋盘格标定（简化版）
        double pixelLen = propertyValue("pixelLength").toDouble();
        double realLen = propertyValue("realLength").toDouble();
        if (pixelLen > 0) {
            ratio = realLen / pixelLen;
        }
    }

    if (ratio <= 0) {
        setResultData("error", "标定比例无效");
        setStatus(ToolStatus::NG);
        return false;
    }

    QString unit = propertyValue("unit").toString();

    // 将标定信息存入上下文，供后续工具使用
    context.setData("calibration_ratio", ratio);
    context.setData("calibration_unit", unit);
    context.setData("calibration_mm_per_pixel", ratio);
    context.setData("calibration_pixel_per_mm", 1.0 / ratio);

    setResultData("mmPerPixel", ratio);
    setResultData("pixelPerMm", 1.0 / ratio);
    setResultData("unit", unit);
    setResultData("calibrated", true);

    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(Calibration, "标定校准", VisionInspector::ToolCategory::Calibration)
