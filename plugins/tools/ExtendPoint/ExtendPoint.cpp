/**
 * @file ExtendPoint.cpp
 * @brief 延伸点工具 (对标 CKVision 延伸点, §2.4 补齐)
 *
 * 从起点沿指定方向角延伸给定长度, 计算终点。
 * 常用于: 直线端点向外延伸一段取参考点/辅助测量点。
 *
 * 输入: startX/startY 起点, angleDeg 方向角(度, 逆时针), length 延伸长度
 *       (各属性可直接填数值或用 "$(工具名.键)" 引用上游输出)
 * 输出: endX/endY 终点 / deltaX/deltaY 增量
 */
#include "ExtendPoint.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList ExtendPoint::propertyDefs() const {
    return {
        PropertyDef::stringProp("startX", "起点X", "0"),
        PropertyDef::stringProp("startY", "起点Y", "0"),
        PropertyDef::stringProp("angleDeg", "方向角(度)", "0"),
        PropertyDef::stringProp("length", "延伸长度", "10"),
    };
}

bool ExtendPoint::execute(ToolContext& /*context*/) {
    const double sx = propertyValue("startX").toDouble();
    const double sy = propertyValue("startY").toDouble();
    const double ang = propertyValue("angleDeg").toDouble() * M_PI / 180.0;
    const double len = propertyValue("length").toDouble();

    const double dx = len * std::cos(ang);
    const double dy = len * std::sin(ang);

    setResultData("endX", sx + dx);
    setResultData("endY", sy + dy);
    setResultData("deltaX", dx);
    setResultData("deltaY", dy);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ExtendPoint, "延伸点", VisionInspector::ToolCategory::Geometry)
