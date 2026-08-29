#include "DistanceMeasure.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList DistanceMeasure::propertyDefs() const {
    return {
        PropertyDef::doubleProp("x1", "点1 X", 0, 0, 100000),
        PropertyDef::doubleProp("y1", "点1 Y", 0, 0, 100000),
        PropertyDef::doubleProp("x2", "点2 X", 100, 0, 100000),
        PropertyDef::doubleProp("y2", "点2 Y", 100, 0, 100000),
        PropertyDef::doubleProp("scale", "标定比例 (mm/pixel)", 0.1, 0.0001, 1000.0),
        PropertyDef::boolProp("useDataLink", "从数据中读取坐标", false),
        PropertyDef::stringProp("dataKeyX1", "数据键-X1", ""),
        PropertyDef::stringProp("dataKeyY1", "数据键-Y1", ""),
        PropertyDef::stringProp("dataKeyX2", "数据键-X2", ""),
        PropertyDef::stringProp("dataKeyY2", "数据键-Y2", ""),
    };
}

bool DistanceMeasure::execute(ToolContext& context) {
    double x1 = propertyValue("x1").toDouble();
    double y1 = propertyValue("y1").toDouble();
    double x2 = propertyValue("x2").toDouble();
    double y2 = propertyValue("y2").toDouble();
    double scale = propertyValue("scale").toDouble();

    if (propertyValue("useDataLink").toBool()) {
        QString kx1 = propertyValue("dataKeyX1").toString();
        QString ky1 = propertyValue("dataKeyY1").toString();
        QString kx2 = propertyValue("dataKeyX2").toString();
        QString ky2 = propertyValue("dataKeyY2").toString();
        if (!kx1.isEmpty() && context.hasData(kx1)) x1 = context.getDouble(kx1);
        if (!ky1.isEmpty() && context.hasData(ky1)) y1 = context.getDouble(ky1);
        if (!kx2.isEmpty() && context.hasData(kx2)) x2 = context.getDouble(kx2);
        if (!ky2.isEmpty() && context.hasData(ky2)) y2 = context.getDouble(ky2);
    }

    double dx = x2 - x1;
    double dy = y2 - y1;
    double pixelDist = std::sqrt(dx * dx + dy * dy);
    double realDist = pixelDist * scale;

    setResultData("distance_pixel", pixelDist);
    setResultData("distance_mm", realDist);
    setResultData("dx", dx);
    setResultData("dy", dy);
    setStatus(ToolStatus::OK);
    return true;
}

VI_REGISTER_TOOL(DistanceMeasure, "距离测量", VisionInspector::ToolCategory::Geometry)
} // namespace VisionInspector
