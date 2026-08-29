#include "AngleMeasure.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList AngleMeasure::propertyDefs() const {
    return {
        PropertyDef::doubleProp("vx", "顶点 X", 100, 0, 100000),
        PropertyDef::doubleProp("vy", "顶点 Y", 100, 0, 100000),
        PropertyDef::doubleProp("p1x", "点1 X", 0, 0, 100000),
        PropertyDef::doubleProp("p1y", "点1 Y", 100, 0, 100000),
        PropertyDef::doubleProp("p2x", "点2 X", 200, 0, 100000),
        PropertyDef::doubleProp("p2y", "点2 Y", 100, 0, 100000),
    };
}

bool AngleMeasure::execute(ToolContext& context) {
    double vx = propertyValue("vx").toDouble();
    double vy = propertyValue("vy").toDouble();
    double p1x = propertyValue("p1x").toDouble();
    double p1y = propertyValue("p1y").toDouble();
    double p2x = propertyValue("p2x").toDouble();
    double p2y = propertyValue("p2y").toDouble();

    double dx1 = p1x - vx, dy1 = p1y - vy;
    double dx2 = p2x - vx, dy2 = p2y - vy;

    double dot = dx1 * dx2 + dy1 * dy2;
    double mag1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    double mag2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

    double angle = 0.0;
    if (mag1 > 0 && mag2 > 0) {
        double cosAngle = dot / (mag1 * mag2);
        cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
        angle = std::acos(cosAngle) * 180.0 / M_PI;
    }

    double angle1 = std::atan2(dy1, dx1) * 180.0 / M_PI;
    double angle2 = std::atan2(dy2, dx2) * 180.0 / M_PI;

    setResultData("angle", angle);
    setResultData("angle1", angle1);
    setResultData("angle2", angle2);
    setResultData("angleDiff", std::abs(angle1 - angle2));
    setStatus(ToolStatus::OK);
    return true;
}

VI_REGISTER_TOOL(AngleMeasure, "角度测量", VisionInspector::ToolCategory::Geometry)
} // namespace VisionInspector
