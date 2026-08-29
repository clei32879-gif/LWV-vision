#include "Loop.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QThread>

namespace VisionInspector {

PropertyDefList Loop::propertyDefs() const {
    return {
        PropertyDef::intProp("loopCount", "循环次数", 10, 1, 100000),
        PropertyDef::stringProp("counterKey", "计数器变量名", "loopIndex"),
    };
}

bool Loop::execute(ToolContext& context) {
    int count = propertyValue("loopCount").toInt();
    QString key = propertyValue("counterKey").toString();

    int current = context.getInt(key, 0);
    int next = current + 1;

    context.setData(key, next);
    setResultData("currentLoop", next);
    setResultData("totalLoops", count);

    if (next >= count) {
        context.setData(key, 0); // Reset for next run
        setResultData("loopComplete", true);
        setStatus(ToolStatus::OK);
    } else {
        setResultData("loopComplete", false);
        context.setMessage(1); // Signal to loop back
        setStatus(ToolStatus::OK);
    }
    return true;
}

VI_REGISTER_TOOL(Loop, "循环", VisionInspector::ToolCategory::Logic)
} // namespace VisionInspector
