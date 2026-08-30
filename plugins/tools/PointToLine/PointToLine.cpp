/**
 * @file PointToLine.cpp
 * @brief 点到线距离工具 (几何测量, 对齐CKVision"点到线")
 *
 * 输入: 点(px,py) 与直线(中心cx,cy + 角度angle, 度)。
 * 支持属性引用: 可填 "$(检测直线.centerX)" 由引擎自动解析。
 * 输出: 垂直距离 distance、垂足坐标 footX/footY、有向距离 signedDistance。
 */
#include "PointToLine.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList PointToLine::propertyDefs() const {
    return {
        PropertyDef::stringProp("pointX", "点X", "320"),
        PropertyDef::stringProp("pointY", "点Y", "240"),
        PropertyDef::stringProp("lineCenterX", "直线中心X", "320"),
        PropertyDef::stringProp("lineCenterY", "直线中心Y", "240"),
        PropertyDef::stringProp("lineAngle", "直线角度(度)", "0"),
    };
}

bool PointToLine::execute(ToolContext& context) {
    const double px = propertyValue("pointX").toDouble();
    const double py = propertyValue("pointY").toDouble();
    const double cx = propertyValue("lineCenterX").toDouble();
    const double cy = propertyValue("lineCenterY").toDouble();
    const double ang = propertyValue("lineAngle").toDouble();

    const double r = ang * M_PI / 180.0;
    // 直线法向量 n = (-sin, cos), 过点(cx,cy): n·(p - c) = 0
    const double nx = -std::sin(r);
    const double ny = std::cos(r);
    const double nlen = std::hypot(nx, ny);
    if (nlen < 1e-12) {
        setResultData("error", "直线方向无效");
        setStatus(ToolStatus::NG);
        return false;
    }

    // 有向距离 = n·(P - C) / |n|
    const double signedDist = (nx * (px - cx) + ny * (py - cy)) / nlen;

    // 垂足 = P - 有向距离 * 单位法向量
    const double footX = px - nx / nlen * signedDist;
    const double footY = py - ny / nlen * signedDist;

    setResultData("distance", std::fabs(signedDist));
    setResultData("signedDistance", signedDist);
    setResultData("footX", footX);
    setResultData("footY", footY);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(PointToLine, "点到线距离", VisionInspector::ToolCategory::Geometry)
