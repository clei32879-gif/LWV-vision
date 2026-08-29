#include "Delay.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QThread>
#include <QElapsedTimer>
#include <QCoreApplication>

namespace VisionInspector {

PropertyDefList Delay::propertyDefs() const {
    return {
        PropertyDef::intProp("delayMs", "延时时间(ms)", 100, 0, 60000),
    };
}

bool Delay::execute(ToolContext& context) {
    int ms = propertyValue("delayMs").toInt();
    if (ms > 0) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < ms) {
            QThread::msleep(10);
            QCoreApplication::processEvents();
        }
    }
    setResultData("actualDelayMs", ms);
    setStatus(ToolStatus::OK);
    return true;
}

VI_REGISTER_TOOL(Delay, "延时", VisionInspector::ToolCategory::Logic)
} // namespace VisionInspector
