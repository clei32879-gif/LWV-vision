/** @file SwitchImage.cpp - 切换图像工具实现
 *
 *  模式:
 *    0 按索引: slotIndex(0-3) 选 namedImage0..namedImage3 中的第 N 张
 *    1 按条件: switchValue 与 caseA/caseB/caseC 比较, 命中哪个就选对应槽 (字符串相等)
 *  选中图像写为 Current, 供后续工具使用; 选中槽为空则 NG。
 */
#include "SwitchImage.h"
#include "../../../src/engine/ToolRegistry.h"

namespace VisionInspector {

PropertyDefList SwitchImage::propertyDefs() const {
    return {
        PropertyDef::enumProp("mode", "选择模式", {"按索引", "按条件匹配"}, 0, "切换"),
        PropertyDef::intProp("slotIndex", "槽位索引(0-3)", 0, 0, 3, "切换"),
        PropertyDef::stringProp("namedImage0", "图像槽0", "", "图像源"),
        PropertyDef::stringProp("namedImage1", "图像槽1", "", "图像源"),
        PropertyDef::stringProp("namedImage2", "图像槽2", "", "图像源"),
        PropertyDef::stringProp("namedImage3", "图像槽3", "", "图像源"),
        PropertyDef::stringProp("switchValue", "切换值(数据链接)", "", "条件"),
        PropertyDef::stringProp("caseA", "槽0匹配值", "", "条件"),
        PropertyDef::stringProp("caseB", "槽1匹配值", "", "条件"),
        PropertyDef::stringProp("caseC", "槽2匹配值", "", "条件"),
    };
}

bool SwitchImage::execute(ToolContext& context) {
    const int mode = propertyValue("mode").toInt();
    QString names[4];
    for (int i = 0; i < 4; ++i)
        names[i] = propertyValue(QString("namedImage%1").arg(i)).toString().trimmed();

    int slot = -1;
    if (mode == 0) {
        slot = propertyValue("slotIndex").toInt();
    } else {
        // 条件匹配: switchValue(可为数据链接, 引擎已解析) 依次比较
        const QString v = propertyValue("switchValue").toString().trimmed();
        const QString cases[3] = {
            propertyValue("caseA").toString().trimmed(),
            propertyValue("caseB").toString().trimmed(),
            propertyValue("caseC").toString().trimmed() };
        for (int i = 0; i < 3; ++i) {
            if (!cases[i].isEmpty() && cases[i] == v) { slot = i; break; }
        }
        setResultData("switchValue", v);
    }

    if (slot < 0 || slot >= 4) {
        setResultData("error", QStringLiteral("条件未命中任何槽 (模式%1)").arg(mode));
        setStatus(ToolStatus::NG); return false;
    }

    CvImagePtr img = context.getImage(names[slot]);
    if (!img || img->empty()) {
        setResultData("error", QStringLiteral("图像槽%1 (%2) 为空或不存在")
            .arg(slot).arg(names[slot].isEmpty() ? QStringLiteral("(未填)") : names[slot]));
        setResultData("selectedSlot", slot);
        setStatus(ToolStatus::NG); return false;
    }

    context.setCurrentImage(img);
    setResultData("selectedSlot", slot);
    setResultData("selectedImage", names[slot]);
    setResultData("width", img->cols);
    setResultData("height", img->rows);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(SwitchImage, "切换图像", VisionInspector::ToolCategory::ImageProcess)
