#include "EndCorrection.h"
#include "../../../src/engine/ToolRegistry.h"

namespace VisionInspector {

PropertyDefList EndCorrection::propertyDefs() const {
    return {
        // 结束补正不需要参数，直接清除坐标系修正
    };
}

bool EndCorrection::execute(ToolContext& context) {
    // 清除坐标系修正，恢复原始坐标
    context.setData("correctedX", 0.0);
    context.setData("correctedY", 0.0);
    context.setData("correctedAngle", 0.0);
    context.setData("coord_cos", 1.0);
    context.setData("coord_sin", 0.0);
    context.setData("coord_originX", 0.0);
    context.setData("coord_originY", 0.0);

    setResultData("status", "坐标系已恢复");
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(EndCorrection, "结束补正", VisionInspector::ToolCategory::Calibration)
