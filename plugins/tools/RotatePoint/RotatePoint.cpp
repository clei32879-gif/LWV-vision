/**
 * @file RotatePoint.cpp
 * @brief 旋转点工具 (对标 CKVision 旋转点, §2.4 补齐)
 *
 * 将点绕旋转中心旋转指定角度, 计算旋转后的坐标。
 * 常用于: 工件旋转后推算某特征点的理论位置/辅助测量点。
 *
 * 输入: pointX/pointY 被旋转点, centerX/centerY 旋转中心,
 *       angleDeg 旋转角(度, 逆时针为正)
 *       (各属性可直接填数值或用 "$(工具名.键)" 引用上游输出)
 * 输出: resultX/resultY 旋转后坐标
 */
#include "RotatePoint.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList RotatePoint::propertyDefs() const {
    return {
        PropertyDef::stringProp("pointX", "点X", "0"),
        PropertyDef::stringProp("pointY", "点Y", "0"),
        PropertyDef::stringProp("centerX", "旋转中心X", "0"),
        PropertyDef::stringProp("centerY", "旋转中心Y", "0"),
        PropertyDef::stringProp("angleDeg", "旋转角(度)", "0"),
    };
}

bool RotatePoint::execute(ToolContext& /*context*/) {
    const double px = propertyValue("pointX").toDouble();
    const double py = propertyValue("pointY").toDouble();
    const double cx = propertyValue("centerX").toDouble();
    const double cy = propertyValue("centerY").toDouble();
    const double ang = propertyValue("angleDeg").toDouble() * M_PI / 180.0;

    const double cosA = std::cos(ang), sinA = std::sin(ang);
    const double rx = cx + (px - cx) * cosA - (py - cy) * sinA;
    const double ry = cy + (px - cx) * sinA + (py - cy) * cosA;

    setResultData("resultX", rx);
    setResultData("resultY", ry);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(RotatePoint, "旋转点", VisionInspector::ToolCategory::Geometry)
