/**
 * @file CircleToCircle.cpp
 * @brief 圆到圆距离工具 (几何测量, 对齐CKVision"圆到圆" + 同心度)
 *
 * 输入: 两圆圆心与半径, 支持 "$(工具.键)" 引用直连检测圆形输出。
 * 输出: 圆心距 centerDistance、边缘间隙 gap (centerDistance - r1 - r2)、
 *       同心度 concentricity (两圆心距)、关系 relation (外离/外切/相交/内切/内含)。
 */
#include "CircleToCircle.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList CircleToCircle::propertyDefs() const {
    return {
        PropertyDef::stringProp("circle1CenterX", "圆1圆心X", "320"),
        PropertyDef::stringProp("circle1CenterY", "圆1圆心Y", "240"),
        PropertyDef::stringProp("circle1Radius", "圆1半径", "100"),
        PropertyDef::stringProp("circle2CenterX", "圆2圆心X", "320"),
        PropertyDef::stringProp("circle2CenterY", "圆2圆心Y", "240"),
        PropertyDef::stringProp("circle2Radius", "圆2半径", "50"),
    };
}

bool CircleToCircle::execute(ToolContext& context) {
    const double c1x = propertyValue("circle1CenterX").toDouble();
    const double c1y = propertyValue("circle1CenterY").toDouble();
    const double r1 = propertyValue("circle1Radius").toDouble();
    const double c2x = propertyValue("circle2CenterX").toDouble();
    const double c2y = propertyValue("circle2CenterY").toDouble();
    const double r2 = propertyValue("circle2Radius").toDouble();
    if (r1 <= 0 || r2 <= 0) {
        setResultData("error", "圆半径无效");
        setStatus(ToolStatus::NG);
        return false;
    }

    const double dist = std::hypot(c2x - c1x, c2y - c1y);
    const double gap = dist - r1 - r2;

    // 关系判定
    QString relation;
    const double eps = 1e-6;
    if (dist > r1 + r2 + eps)      relation = QStringLiteral("外离");
    else if (std::fabs(dist - (r1 + r2)) <= eps) relation = QStringLiteral("外切");
    else if (dist < std::fabs(r1 - r2) - eps)    relation = QStringLiteral("内含");
    else if (std::fabs(dist - std::fabs(r1 - r2)) <= eps) relation = QStringLiteral("内切");
    else                             relation = QStringLiteral("相交");

    setResultData("centerDistance", dist);
    setResultData("gap", gap);
    setResultData("concentricity", dist);   // 同心度 = 两圆心距
    setResultData("relation", relation);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CircleToCircle, "圆到圆距离", VisionInspector::ToolCategory::Geometry)
