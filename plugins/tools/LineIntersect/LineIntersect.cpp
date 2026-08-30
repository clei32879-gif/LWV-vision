/**
 * @file LineIntersect.cpp
 * @brief 两线交点工具 (几何测量, 对齐CKVision"线到线")
 *
 * 输入: 两条直线的中心(centerX/Y)与方向角(angle, 度)。
 * 支持属性引用: 可填 "$(检测直线.centerX)" 由引擎自动解析上游工具输出。
 * 输出: 交点坐标 intersectX/Y、两线夹角 angleBetween、平行标记 parallel。
 */
#include "LineIntersect.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList LineIntersect::propertyDefs() const {
    return {
        PropertyDef::stringProp("line1CenterX", "直线1中心X", "320"),
        PropertyDef::stringProp("line1CenterY", "直线1中心Y", "240"),
        PropertyDef::stringProp("line1Angle", "直线1角度(度)", "0"),
        PropertyDef::stringProp("line2CenterX", "直线2中心X", "320"),
        PropertyDef::stringProp("line2CenterY", "直线2中心Y", "240"),
        PropertyDef::stringProp("line2Angle", "直线2角度(度)", "90"),
    };
}

bool LineIntersect::execute(ToolContext& context) {
    // 属性可为数值或 "$(工具.键)" 引用 (引擎PropertyResolveGuard已解析)
    const double cx1 = propertyValue("line1CenterX").toDouble();
    const double cy1 = propertyValue("line1CenterY").toDouble();
    const double a1 = propertyValue("line1Angle").toDouble();
    const double cx2 = propertyValue("line2CenterX").toDouble();
    const double cy2 = propertyValue("line2CenterY").toDouble();
    const double a2 = propertyValue("line2Angle").toDouble();

    const double r1 = a1 * M_PI / 180.0, r2 = a2 * M_PI / 180.0;
    const double d1x = std::cos(r1), d1y = std::sin(r1);
    const double d2x = std::cos(r2), d2y = std::sin(r2);

    // 方向向量叉积 → 平行判定
    const double denom = d1x * d2y - d1y * d2x;
    const double eps = 1e-9;

    if (std::fabs(denom) < eps) {
        // 平行/重合: 无唯一交点
        setResultData("parallel", true);
        setResultData("angleBetween", 0.0);
        setResultData("intersectX", 0.0);
        setResultData("intersectY", 0.0);
        setStatus(ToolStatus::OK);
        return true;
    }

    // 求交点: c1 + t*d1 = c2 + s*d2 → t = cross(c2-c1, d2) / cross(d1,d2)
    const double t = ((cx2 - cx1) * d2y - (cy2 - cy1) * d2x) / denom;
    const double px = cx1 + t * d1x;
    const double py = cy1 + t * d1y;

    // 两线夹角(锐角, 度): 取方向向量夹角, 折叠到[0,90]
    double ang = std::atan2(std::fabs(d1x * d2y - d1y * d2x),
                            d1x * d2x + d1y * d2y) * 180.0 / M_PI;
    if (ang > 90.0) ang = 180.0 - ang;

    setResultData("intersectX", px);
    setResultData("intersectY", py);
    setResultData("angleBetween", ang);
    setResultData("parallel", false);
    setResultData("found", true);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(LineIntersect, "两线交点", VisionInspector::ToolCategory::Geometry)
