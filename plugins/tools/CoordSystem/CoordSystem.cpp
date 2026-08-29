#include "CoordSystem.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList CoordSystem::propertyDefs() const {
    return {
        PropertyDef::enumProp("mode", "组合类型", {"原点和角度", "原点和方向点"}, 0),
        PropertyDef::doubleProp("originX", "原点X", 0, 0, 10000),
        PropertyDef::doubleProp("originY", "原点Y", 0, 0, 10000),
        PropertyDef::doubleProp("angle", "角度(度)", 0, -180, 180),
        PropertyDef::doubleProp("dirX", "方向点X", 100, 0, 10000),
        PropertyDef::doubleProp("dirY", "方向点Y", 0, 0, 10000),
        PropertyDef::stringProp("originXKey", "原点X数据链接", ""),
        PropertyDef::stringProp("originYKey", "原点Y数据链接", ""),
        PropertyDef::stringProp("angleKey", "角度数据链接", ""),
    };
}

bool CoordSystem::execute(ToolContext& context) {
    int mode = propertyValue("mode").toInt();
    double originX = propertyValue("originX").toDouble();
    double originY = propertyValue("originY").toDouble();
    double angle = propertyValue("angle").toDouble();
    // 从数据链接获取值
    QString oxKey = propertyValue("originXKey").toString();
    QString oyKey = propertyValue("originYKey").toString();
    QString aKey = propertyValue("angleKey").toString();
    if (!oxKey.isEmpty() && context.hasData(oxKey)) originX = context.getDouble(oxKey);
    if (!oyKey.isEmpty() && context.hasData(oyKey)) originY = context.getDouble(oyKey);
    if (!aKey.isEmpty() && context.hasData(aKey)) angle = context.getDouble(aKey);
    if (mode == 1) {
        // 原点和方向点模式
        double dirX = propertyValue("dirX").toDouble();
        double dirY = propertyValue("dirY").toDouble();
        angle = std::atan2(dirY - originY, dirX - originX) * 180.0 / M_PI;
    }
    // 存储坐标系参数到上下文
    context.setData("coord_originX", originX);
    context.setData("coord_originY", originY);
    context.setData("coord_angle", angle);
    double rad = angle * M_PI / 180.0;
    context.setData("coord_cos", std::cos(rad));
    context.setData("coord_sin", std::sin(rad));
    setResultData("originX", originX);
    setResultData("originY", originY);
    setResultData("angle", angle);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CoordSystem, "坐标系统", VisionInspector::ToolCategory::Calibration)
