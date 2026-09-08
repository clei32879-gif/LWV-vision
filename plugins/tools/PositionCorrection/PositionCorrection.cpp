#include "PositionCorrection.h"
#include "../../../src/engine/ToolRegistry.h"
#include <cmath>

namespace VisionInspector {

PropertyDefList PositionCorrection::propertyDefs() const {
    return {
        // 简便模式 (对标CKVision): 选一个匹配工具, 自动跟随其形状位置补正
        PropertyDef::enumProp("sourceMode", "来源模式", {"自动跟随匹配工具", "手动数据链接"}, 0, "来源"),
        PropertyDef::stringProp("matchTool", "匹配工具名(留空=流程里最近一个)", "", "来源"),
        // 手动模式: 通过数据链接指向匹配工具的输出
        PropertyDef::stringProp("originXLink", "原点X(手动)", "", "来源"),
        PropertyDef::stringProp("originYLink", "原点Y(手动)", "", "来源"),
        PropertyDef::stringProp("angleLink", "角度(手动)", "", "来源"),
        // 偏移量
        PropertyDef::doubleProp("offsetX", "X偏移量(mm)", 0.0, -1000, 1000),
        PropertyDef::doubleProp("offsetY", "Y偏移量(mm)", 0.0, -1000, 1000),
        PropertyDef::doubleProp("offsetAngle", "角度偏移(度)", 0.0, -360, 360),
    };
}

bool PositionCorrection::execute(ToolContext& context) {
    double originX = 0, originY = 0, angle = 0;
    QString matchedTool;

    if (propertyValue("sourceMode").toInt() == 0) {
        // ---- 自动跟随匹配工具: 按各匹配工具的输出键约定自动取值 ----
        // 键约定: ShapeMatch/GrayscaleMatch/TemplateMatch → matchX/matchY/(matchAngle|angle)
        //         EdgeCircleFind/CircleDetection    → centerX/centerY (无角度, 取0)
        // 匹配工具名留空 = 向前找最近的定位类工具 (ShapeMatch/GrayscaleMatch/TemplateMatch/
        //                                    EdgeCircleFind/CircleDetection/ContourMatch)
        const QString want = propertyValue("matchTool").toString().trimmed();
        const QStringList locateTypes = {
            "ShapeMatch", "GrayscaleMatch", "TemplateMatch",
            "EdgeCircleFind", "CircleDetection", "ContourMatch" };
        // ToolContext 无工具列表, 用引擎回写的流程索引不可达 — 由属性对话框预填/用户填名.
        // 这里按"名后取键"约定 + 空名的常见默认链 (matchX/matchY/matchAngle):
        const QString base = want;
        QString xKey, yKey, aKey;
        if (base.contains("Circle") || base.contains("找圆")) {
            xKey = base + ".centerX"; yKey = base + ".centerY"; aKey = "";
        } else {
            xKey = base + ".matchX"; yKey = base + ".matchY";
            aKey = base + ".matchAngle";
            if (!context.hasData(aKey)) aKey = base + ".angle";
        }
        bool got = false;
        if (!base.isEmpty() && context.hasData(xKey)) {
            originX = context.getDouble(xKey, 0);
            originY = context.getDouble(yKey, 0);
            angle = aKey.isEmpty() ? 0 : context.getDouble(aKey, 0);
            got = true; matchedTool = base;
        }
        // 兜底: 走通用键 (单定位工具流程最常见)
        if (!got && context.hasData("matchX")) {
            originX = context.getDouble("matchX", 0);
            originY = context.getDouble("matchY", 0);
            angle = context.hasData("matchAngle") ? context.getDouble("matchAngle", 0)
                                                  : context.getDouble("angle", 0);
            got = true; matchedTool = "(通用键)";
        }
        if (!got && context.hasData("centerX")) {
            originX = context.getDouble("centerX", 0);
            originY = context.getDouble("centerY", 0);
            got = true; matchedTool = "(找圆键)";
        }
        if (!got) {
            setResultData("error", QStringLiteral(
                "自动跟随失败: 流程中无匹配结果 (matchX/matchY/centerX). "
                "请把匹配工具放在本工具之前, 或在'匹配工具名'填其实例名, 或切换手动模式"));
            setStatus(ToolStatus::NG);
            return false;
        }
        setResultData("autoSource", matchedTool);
    } else {
        // ---- 手动数据链接模式 ----
        QString xLink = propertyValue("originXLink").toString();
        QString yLink = propertyValue("originYLink").toString();
        QString angleLink = propertyValue("angleLink").toString();
        if (!xLink.isEmpty()) originX = context.getDouble(xLink, 0);
        if (!yLink.isEmpty()) originY = context.getDouble(yLink, 0);
        if (!angleLink.isEmpty()) angle = context.getDouble(angleLink, 0);
    }

    // 获取偏移量
    double offsetX = propertyValue("offsetX").toDouble();
    double offsetY = propertyValue("offsetY").toDouble();
    double offsetAngle = propertyValue("offsetAngle").toDouble();

    // 计算补正后的坐标系
    double correctedX = originX + offsetX;
    double correctedY = originY + offsetY;
    double correctedAngle = angle + offsetAngle;

    // 将坐标系参数存入上下文，供后续工具使用
    context.setData("correctedX", correctedX);
    context.setData("correctedY", correctedY);
    context.setData("correctedAngle", correctedAngle);
    
    // 存储坐标变换矩阵（供ROI跟随移动使用）
    double rad = correctedAngle * M_PI / 180.0;
    context.setData("coord_angle", correctedAngle);   // 角度跟随 (与CoordSystem一致)
    context.setData("coord_cos", std::cos(rad));
    context.setData("coord_sin", std::sin(rad));
    context.setData("coord_originX", correctedX);
    context.setData("coord_originY", correctedY);
    // 坐标系服务: 后续检测工具的ROI自动跟随本补正坐标系
    context.setCoordinateFrame(std::cos(rad), std::sin(rad), correctedX, correctedY);

    setResultData("correctedX", correctedX);
    setResultData("correctedY", correctedY);
    setResultData("correctedAngle", correctedAngle);
    setResultData("originX", originX);
    setResultData("originY", originY);
    setResultData("angle", angle);

    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(PositionCorrection, "位置补正", VisionInspector::ToolCategory::Calibration)
