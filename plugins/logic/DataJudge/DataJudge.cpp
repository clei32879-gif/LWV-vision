#include "DataJudge.h"
#include "../../../src/engine/ToolRegistry.h"

namespace VisionInspector {

PropertyDefList DataJudge::propertyDefs() const {
    return {
        PropertyDef::enumProp("judgeType", "判断类型",
            {"大于", "小于", "范围内", "范围外"}, 0),
        PropertyDef::doubleProp("upperLimit", "上限值", 0.0),
        PropertyDef::doubleProp("lowerLimit", "下限值", 0.0),
        PropertyDef::stringProp("sourceDataKey", "输入数据Key", ""),
    };
}

bool DataJudge::execute(ToolContext& context) {
    int judgeType = propertyValue("judgeType").toInt();
    double upperLimit = propertyValue("upperLimit").toDouble();
    double lowerLimit = propertyValue("lowerLimit").toDouble();
    QString dataKey = propertyValue("sourceDataKey").toString();

    QVariant rawValue;
    if (!dataKey.isEmpty()) {
        rawValue = context.getData(dataKey);
    } else {
        auto all = context.allData();
        if (!all.isEmpty()) rawValue = all.last();
    }

    double value = rawValue.toDouble();

    bool ok = false;
    switch (judgeType) {
    case 0: ok = (value > lowerLimit);  break;
    case 1: ok = (value < upperLimit);  break;
    case 2: ok = (value >= lowerLimit && value <= upperLimit); break;
    case 3: ok = (value < lowerLimit || value > upperLimit); break;
    }

    QString prefix = QString("__DataJudge_%1_").arg(instanceName());
    int okCount = context.getInt(prefix + "okCount", 0);
    int ngCount = context.getInt(prefix + "ngCount", 0);

    if (ok) okCount++;
    else ngCount++;

    context.setData(prefix + "okCount", okCount);
    context.setData(prefix + "ngCount", ngCount);

    double okRate = (okCount + ngCount) > 0
        ? (double)okCount / (okCount + ngCount) * 100.0 : 0.0;

    setResultData("judgment", ok ? "OK" : "NG");
    setResultData("value", value);
    setResultData("okCount", okCount);
    setResultData("ngCount", ngCount);
    setResultData("okRate", okRate);

    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return true;
}

VI_REGISTER_TOOL(DataJudge, "数据判断", VisionInspector::ToolCategory::Logic)
} // namespace VisionInspector
