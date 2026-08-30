/**
 * @file LineToLine.cpp
 * @brief 线到线测量工具 (对标 CKVision 线到线, §2.4 补齐)
 *
 * 计算两条直线的间距与夹角。
 * - 距离: 直线1中心点到直线2的垂直距离 (平行线时即精确间距)
 * - 夹角: 两线夹角(锐角折叠到[0,90])
 * - 平行判定: 夹角接近0/180 视为平行
 *
 * 输入沿用"直线中心+角度"表示法, 各属性可直接填数值或用
 * "$(工具名.键)" 引用上游(检测直线)输出, 由引擎在执行前解析。
 *
 * 输出: distance / angleBetween / parallel / found
 */
#include "LineToLine.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList LineToLine::propertyDefs() const {
    return {
        PropertyDef::stringProp("line1CenterX", "直线1中心X", "0"),
        PropertyDef::stringProp("line1CenterY", "直线1中心Y", "0"),
        PropertyDef::stringProp("line1Angle", "直线1角度(度)", "0"),
        PropertyDef::stringProp("line2CenterX", "直线2中心X", "0"),
        PropertyDef::stringProp("line2CenterY", "直线2中心Y", "0"),
        PropertyDef::stringProp("line2Angle", "直线2角度(度)", "0"),
    };
}

bool LineToLine::execute(ToolContext& context) {
    const double cx1 = propertyValue("line1CenterX").toDouble();
    const double cy1 = propertyValue("line1CenterY").toDouble();
    const double a1 = propertyValue("line1Angle").toDouble();
    const double cx2 = propertyValue("line2CenterX").toDouble();
    const double cy2 = propertyValue("line2CenterY").toDouble();
    const double a2 = propertyValue("line2Angle").toDouble();

    const double r1 = a1 * M_PI / 180.0, r2 = a2 * M_PI / 180.0;
    const double d2x = std::cos(r2), d2y = std::sin(r2);
    // 直线2法线 (单位向量)
    const double n2x = -d2y, n2y = d2x;

    // 线1中心到线2的垂直距离
    const double dist = std::fabs((cx1 - cx2) * n2x + (cy1 - cy2) * n2y);

    // 两线夹角(锐角, 度)
    const double d1x = std::cos(r1), d1y = std::sin(r1);
    double ang = std::atan2(std::fabs(d1x * d2y - d1y * d2x),
                            d1x * d2x + d1y * d2y) * 180.0 / M_PI;
    if (ang > 90.0) ang = 180.0 - ang;
    const bool parallel = ang < 0.5 || ang > 179.5;

    setResultData("distance", dist);
    setResultData("angleBetween", ang);
    setResultData("parallel", parallel);
    setResultData("found", true);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(LineToLine, "线到线", VisionInspector::ToolCategory::Geometry)
