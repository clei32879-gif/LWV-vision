/**
 * @file CircleToLine.cpp
 * @brief 圆到线距离工具 (几何测量, 对齐CKVision"线到圆")
 *
 * 输入: 圆心(cx,cy) 与直线(中心lx,ly + 角度angle, 度)。
 * 支持属性引用: 可填 "$(检测直线.centerX)" / "$(检测圆形.centerX)" 由引擎自动解析。
 * 输出: 圆心到直线的垂直距离 distance、最近点 footX/footY、有向距离 signedDistance。
 */
#include "CircleToLine.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList CircleToLine::propertyDefs() const {
    return {
        PropertyDef::stringProp("circleCenterX", "圆心X", "320"),
        PropertyDef::stringProp("circleCenterY", "圆心Y", "240"),
        PropertyDef::stringProp("lineCenterX", "直线中心X", "320"),
        PropertyDef::stringProp("lineCenterY", "直线中心Y", "240"),
        PropertyDef::stringProp("lineAngle", "直线角度(度)", "0"),
    };
}

bool CircleToLine::execute(ToolContext& context) {
    const double cx = propertyValue("circleCenterX").toDouble();
    const double cy = propertyValue("circleCenterY").toDouble();
    const double lx = propertyValue("lineCenterX").toDouble();
    const double ly = propertyValue("lineCenterY").toDouble();
    const double ang = propertyValue("lineAngle").toDouble();

    const double r = ang * M_PI / 180.0;
    // 直线法向量 n = (-sin, cos), 过点(lx,ly): n·(p - l) = 0
    const double nx = -std::sin(r);
    const double ny = std::cos(r);
    const double nlen = std::hypot(nx, ny);
    if (nlen < 1e-12) {
        setResultData("error", "直线方向无效");
        setStatus(ToolStatus::NG);
        return false;
    }

    // 有向距离 = n·(C - L) / |n|
    const double signedDist = (nx * (cx - lx) + ny * (cy - ly)) / nlen;

    // 垂足 = 圆心 - 有向距离 * 单位法向量
    const double footX = cx - nx / nlen * signedDist;
    const double footY = cy - ny / nlen * signedDist;

    setResultData("distance", std::fabs(signedDist));
    setResultData("signedDistance", signedDist);
    setResultData("footX", footX);
    setResultData("footY", footY);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CircleToLine, "圆到线距离", VisionInspector::ToolCategory::Geometry)
