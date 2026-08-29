#include "DataDisplay.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList DataDisplay::propertyDefs() const {
    return {
        PropertyDef::stringProp("text", "显示文本", "Result: %1"),
        PropertyDef::stringProp("dataKey", "数据链接", ""),
        PropertyDef::intProp("posX", "X坐标", 10, 0, 10000),
        PropertyDef::intProp("posY", "Y坐标", 30, 0, 10000),
        PropertyDef::doubleProp("fontSize", "字体大小", 1.0, 0.5, 5.0),
        PropertyDef::intProp("thickness", "字体粗细", 2, 1, 5),
        PropertyDef::enumProp("color", "颜色", {"白色", "绿色", "红色", "黄色", "蓝色"}, 0),
        PropertyDef::boolProp("showStatus", "显示OK/NG状态", true),
    };
}

bool DataDisplay::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    cv::Mat display;
    if (input->channels() == 1) cv::cvtColor(*input, display, cv::COLOR_GRAY2BGR);
    else display = input->clone();

    // 获取要显示的数据
    QString text = propertyValue("text").toString();
    QString dataKey = propertyValue("dataKey").toString();
    if (!dataKey.isEmpty() && context.hasData(dataKey)) {
        text = text.arg(context.getData(dataKey).toString());
    }

    int x = propertyValue("posX").toInt();
    int y = propertyValue("posY").toInt();
    double scale = propertyValue("fontSize").toDouble();
    int thick = propertyValue("thickness").toInt();

    // 颜色选择
    int colorIdx = propertyValue("color").toInt();
    cv::Scalar color;
    switch (colorIdx) {
    case 0: color = cv::Scalar(255, 255, 255); break; // 白
    case 1: color = cv::Scalar(0, 255, 0); break;     // 绿
    case 2: color = cv::Scalar(0, 0, 255); break;     // 红
    case 3: color = cv::Scalar(0, 255, 255); break;   // 黄
    case 4: color = cv::Scalar(255, 0, 0); break;     // 蓝
    default: color = cv::Scalar(255, 255, 255); break;
    }

    cv::putText(display, text.toStdString(), cv::Point(x, y),
                cv::FONT_HERSHEY_SIMPLEX, scale, color, thick);

    // 显示OK/NG状态
    if (propertyValue("showStatus").toBool()) {
        QString statusText;
        cv::Scalar statusColor;
        if (context.hasData("overallResult") && context.getBool("overallResult")) {
            statusText = "OK";
            statusColor = cv::Scalar(0, 255, 0);
        } else {
            statusText = "NG";
            statusColor = cv::Scalar(0, 0, 255);
        }
        cv::putText(display, statusText.toStdString(), cv::Point(x, y + 40),
                    cv::FONT_HERSHEY_SIMPLEX, scale * 1.5, statusColor, thick + 1);
    }

    setOutputImage(context, std::make_shared<CvImage>(display.clone()));
    setResultData("displayText", text);
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(DataDisplay, "数据显示", VisionInspector::ToolCategory::Special)
