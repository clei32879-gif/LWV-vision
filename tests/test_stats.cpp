/**
 * @file test_stats.cpp
 * @brief 检测结果统计/记录/CSV导出 回归测试
 *
 * 覆盖: GlobalStats 计数/良率/检测项统计/历史记录/CSV导出,
 *       FlowEngine 最近一次执行的结果数据与工具状态快照。
 * 无头运行, 全部通过返回0。
 */

#include "../../src/engine/InspectionResult.h"
#include "../../src/engine/FlowEngine.h"
#include "../../src/engine/ToolRegistry.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [OK] %s\n", msg); } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

// ---- 测试工具: 输出数值, 可配置通过/NG ----
class StatTool : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return "StatTool"; }
    QString displayName() const override { return "统计工具"; }
    ToolCategory category() const override { return ToolCategory::System; }
    PropertyDefList propertyDefs() const override {
        return { PropertyDef::doubleProp("value", "输出值", 1.0),
                 PropertyDef::boolProp("pass", "判定通过", true) };
    }
    bool execute(ToolContext& context) override {
        setResultData("value", propertyValue("value").toDouble());
        return propertyValue("pass").toBool();
    }
};

#include "test_stats.moc"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // ---- 测试1: GlobalStats 计数/良率/检测项统计 ----
    std::printf("测试1: GlobalStats 统计\n");
    GlobalStats stats;
    int okCount = 0, ngCount = 0;
    for (int i = 0; i < 10; ++i) {
        InspectionRecord r;
        r.index = i + 1;
        r.timestamp = QDateTime::currentDateTime();
        r.overallOk = (i % 2 == 0);   // 5 OK, 5 NG
        r.itemResults["外径"] = r.overallOk;
        r.itemResults["牙数"] = (i < 8);   // 8 OK, 2 NG
        r.values["外径"] = 10.0 + i;
        if (i == 0) r.isRetest = true;        if (r.overallOk) okCount++; else ngCount++;
        stats.addRecord(r);
    }
    CHECK(stats.totalCount() == 10, "总计数=10");
    CHECK(stats.passCount() == 5, "通过数=5");
    CHECK(stats.failCount() == 5, "NG数=5");
    CHECK(stats.retestCount() == 1, "重测数=1");
    CHECK(qAbs(stats.yieldRate() - 50.0) < 0.001, "良率=50%");
    CHECK(stats.itemStats("牙数").okCount == 8, "牙数检测项 OK=8");
    CHECK(stats.itemStats("牙数").ngCount == 2, "牙数检测项 NG=2");

    // ---- 测试2: 历史记录 ----
    std::printf("测试2: 历史记录\n");
    CHECK(stats.historyCount() == 10, "历史记录条数=10");
    CHECK(stats.records().first().index == 1, "首条序号=1");
    CHECK(stats.records().last().index == 10, "末条序号=10");

    // ---- 测试3: CSV 导出 ----
    std::printf("测试3: CSV 导出\n");
    QTemporaryDir tmp;
    CHECK(tmp.isValid(), "临时目录可用");
    const QString csvPath = tmp.filePath("records.csv");
    QString err;
    CHECK(stats.exportCsv(csvPath, &err), "exportCsv 返回true");
    QFile f(csvPath);
    CHECK(f.open(QIODevice::ReadOnly), "CSV 文件可读");
    const QByteArray bytes = f.readAll();
    f.close();
    CHECK(bytes.size() > 3, "CSV 非空");
    CHECK((quint8)bytes.at(0) == 0xEF && (quint8)bytes.at(1) == 0xBB && (quint8)bytes.at(2) == 0xBF,
          "CSV 含 UTF-8 BOM (Excel 中文兼容)");
    const QString text = QString::fromUtf8(bytes);
    CHECK(text.contains("序号") && text.contains("结果") && text.contains("数值明细"),
          "CSV 表头完整");
    CHECK(text.contains("OK") && text.contains("NG"), "CSV 含 OK/NG 行");
    // 10 条记录 + 1 表头
    CHECK(text.count('\n') == 11, "CSV 行数=11 (1表头+10记录)");

    // ---- 测试4: FlowEngine 结果快照 ----
    std::printf("测试4: FlowEngine 结果/工具状态快照\n");
    auto& reg = ToolRegistry::instance();
    reg.registerTool("StatTool", {"StatTool", "统计工具", ToolCategory::System,
                                  []() { return new StatTool(); }});
    FlowEngine engine;
    Flow* flow = new Flow(&engine);
    flow->setName("统计流程");
    auto* t1 = static_cast<StatTool*>(reg.createTool("StatTool"));
    t1->setInstanceName("外径测量");
    t1->setProperty("value", 12.5);
    auto* t2 = static_cast<StatTool*>(reg.createTool("StatTool"));
    t2->setInstanceName("牙数检测");
    t2->setProperty("value", 24.0);
    t2->setProperty("pass", false);   // 第二工具 NG
    flow->addTool(t1);
    flow->addTool(t2);
    engine.addFlow(flow);

    ToolContext ctx;
    const bool allOk = engine.executeOnce(flow, ctx);
    CHECK(!allOk, "含NG工具的流程返回 false");
    const DataMap rd = engine.lastResultData();
    CHECK(qAbs(rd.value("外径测量.value").toDouble() - 12.5) < 1e-9,
          "结果数据含 外径测量.value=12.5");
    CHECK(qAbs(rd.value("牙数检测.value").toDouble() - 24.0) < 1e-9,
          "结果数据含 牙数检测.value=24.0");
    const auto states = engine.lastToolStates();
    CHECK(states.size() == 2, "工具状态快照含2条");
    CHECK(states[0].first == "外径测量" && states[0].second == true, "工具1 外径测量 OK");
    CHECK(states[1].first == "牙数检测" && states[1].second == false, "工具2 牙数检测 NG");

    std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
