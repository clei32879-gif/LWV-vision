#include "PositionCorrection.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList PositionCorrection::propertyDefs() const {
    return {
        // 原点X/Y/角度 — 通过数据链接指向形状匹配的输出
        PropertyDef::stringProp("originXLink", "原点X", ""),
        PropertyDef::stringProp("originYLink", "原点Y", ""),
        PropertyDef::stringProp("angleLink", "角度", ""),
        // 偏移量
        PropertyDef::doubleProp("offsetX", "X偏移量(mm)", 0.0, -1000, 1000),
        PropertyDef::doubleProp("offsetY", "Y偏移量(mm)", 0.0, -1000, 1000),
        PropertyDef::doubleProp("offsetAngle", "角度偏移(度)", 0.0, -360, 360),
    };
}

bool PositionCorrection::execute(ToolContext& context) {
    // 从数据链接获取形状匹配的结果
    QString xLink = propertyValue("originXLink").toString();
    QString yLink = propertyValue("originYLink").toString();
    QString angleLink = propertyValue("angleLink").toString();

    double originX = 0, originY = 0, angle = 0;
    
    // 从上下文中获取链接的数据
    if (!xLink.isEmpty()) originX = context.getDouble(xLink, 0);
    if (!yLink.isEmpty()) originY = context.getDouble(yLink, 0);
    if (!angleLink.isEmpty()) angle = context.getDouble(angleLink, 0);

    // 获取偏移量
    double offsetX = propertyValue("offsetX").toDouble();
    double offsetY = propertyValue("offsetY").toDouble();
    double offsetAngle = propertyValue("offsetAngle").toDouble();

    // 计算补正后的坐标系
    double correctedX = originX + offsetX;
    double correctedY = originY + offsetY;
    double correctedAngle = angle + offsetAngle;

    // 将坐标系参数存入上下文，供后续工具使用
    context.setData("correctedX", correctedX);
    context.setData("correctedY", correctedY);
    context.setData("correctedAngle", correctedAngle);
    
    // 存储坐标变换矩阵（供ROI跟随移动使用）
    double rad = correctedAngle * M_PI / 180.0;
    context.setData("coord_angle", correctedAngle);   // 角度跟随 (与CoordSystem一致)
    context.setData("coord_cos", std::cos(rad));
    context.setData("coord_sin", std::sin(rad));
    context.setData("coord_originX", correctedX);
    context.setData("coord_originY", correctedY);

    setResultData("correctedX", correctedX);
    setResultData("correctedY", correctedY);
    setResultData("correctedAngle", correctedAngle);
    setResultData("originX", originX);
    setResultData("originY", originY);
    setResultData("angle", angle);

    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(PositionCorrection, "位置补正", VisionInspector::ToolCategory::Calibration)
