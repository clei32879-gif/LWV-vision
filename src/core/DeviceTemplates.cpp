/** @file DeviceTemplates.cpp - 内置设备模板定义与构建 */
#include "DeviceTemplates.h"
#include "../engine/FlowEngine.h"
#include "../engine/ToolRegistry.h"
#include "../utils/Logger.h"

#include <QMap>

namespace VisionInspector {

const QList<DeviceTemplate>& DeviceTemplates::all() {
    static const QList<DeviceTemplate> kTemplates = {
        // ---- 视觉筛选 (8工位转盘筛选机标准工序) ----
        { QStringLiteral("sifter"), QStringLiteral("筛选机模板"),
          QStringLiteral("8工位转盘筛选机标准工序: 采集→预处理→定位→补正→检测组→结束补正→变量→判断→显示"),
          { "CaptureImage", "ImageFilter", "ShapeMatch",
            "PositionCorrection", "BlobAnalysis", "VertexDetection",
            "EdgeDetection", "DistanceMeasure", "LineDetection",
            "CircleDetection", "Caliper",
            "ThreadInspection",
            "EndCorrection", "CalculateVariable", "SetVariable",
            "DataJudge", "DataDisplay", "UpdateView" } },
        // ---- 视觉贴标 (定位→算偏移→输出) ----
        { QStringLiteral("labeling"), QStringLiteral("视觉贴标模板"),
          QStringLiteral("贴标机定位工序: 采集→预处理→形状匹配定位→位置补正→计算贴标偏移→判定→显示"),
          { "CaptureImage", "ImageFilter", "ShapeMatch",
            "PositionCorrection", "EndCorrection",
            "CalculateVariable", "DataJudge", "DataDisplay", "UpdateView" } },
        // ---- 视觉计数 (二值化→斑点计数→判定) ----
        { QStringLiteral("counting"), QStringLiteral("视觉计数模板"),
          QStringLiteral("数量检查工序: 采集→预处理→阈值分割→形态学修饰→斑点计数→判定→显示"),
          { "CaptureImage", "ImageFilter", "Threshold",
            "Morphology", "BlobAnalysis", "DataJudge", "DataDisplay", "UpdateView" } },
        // ---- 木业检测 (色差/节疤外观) ----
        { QStringLiteral("wood"), QStringLiteral("木业检测模板"),
          QStringLiteral("木制品外观工序: 采集→预处理→颜色识别→斑点分类(节疤/色斑)→判定→显示"),
          { "CaptureImage", "ImageFilter", "ColorDetection",
            "BlobClassify", "DataJudge", "DataDisplay", "UpdateView" } },
    };
    return kTemplates;
}

const DeviceTemplate* DeviceTemplates::byId(const QString& id) {
    for (const DeviceTemplate& t : all())
        if (t.id == id) return &t;
    return nullptr;
}

bool DeviceTemplates::build(const QString& id, FlowEngine* engine, int* createdCount) {
    const DeviceTemplate* tpl = byId(id);
    if (!tpl || !engine) {
        VI_LOG_ERROR(QString("设备模板不存在: %1").arg(id));
        return false;
    }

    Flow* flow = new Flow(engine);
    flow->setName(QStringLiteral("主流程"));
    engine->addFlow(flow);

    QMap<QString, int> counts;
    int created = 0;
    for (const QString& typeName : tpl->tools) {
        ITool* tool = ToolRegistry::instance().createTool(typeName);
        if (!tool) {
            VI_LOG_WARN(QString("模板工具缺失, 已跳过: %1").arg(typeName));
            continue;
        }
        const QString disp = tool->displayName();
        const int n = counts[disp]++;
        tool->setInstanceName(n == 0 ? disp : QString("%1_%2").arg(disp).arg(n + 1));
        flow->addTool(tool);
        ++created;
    }
    if (createdCount) *createdCount = created;
    VI_LOG_INFO(QString("设备模板[%1]已构建: %2个工具").arg(tpl->name).arg(created));
    return true;
}

} // namespace VisionInspector
