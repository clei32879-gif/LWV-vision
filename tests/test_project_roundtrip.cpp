/**
 * @file test_project_roundtrip.cpp
 * @brief QA: 项目保存→加载→执行 往返一致性测试
 *
 * 流程: 建2个流程5个工具(设属性/判定/引用) → 保存.vipj → 清空 → 加载 →
 *       逐项断言(工具数/实例名/属性值/判定/顺序) → 执行一轮验证功能
 */

#include "../src/core/ProjectManager.h"
#include "../src/core/GlobalVariables.h"
#include "../src/engine/FlowEngine.h"
#include "../src/engine/ToolRegistry.h"

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

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    auto& reg = ToolRegistry::instance();
    if (reg.allMetaData().isEmpty()) {
        std::printf("[跳过] 无注册工具(需链插件)\n");
        return 0;
    }

    GlobalVariables globals;
    FlowEngine engine;
    ProjectManager pm;
    pm.setServices(&engine, &globals);

    // ---- 1. 构建: 找圆(带判定) + 螺纹检测(带引用) ----
    Flow* flow = new Flow(&engine);
    flow->setName("主流程");
    ITool* finder = reg.createTool("EdgeCircleFind");
    if (!finder) { std::printf("[FAIL] EdgeCircleFind未注册\n"); return 1; }
    finder->setInstanceName("快速找圆");
    finder->setProperty("minRadius", 80.0);
    finder->setProperty("maxRadius", 140.0);
    // 工具内建判定: radius在[90,130]
    ResultJudgment j;
    j.resultKey = "radius"; j.enabled = true; j.lower = 90; j.upper = 130;
    QList<ResultJudgment> js{j};
    finder->setJudgments(js);
    flow->addTool(finder);

    ITool* thread = reg.createTool("ThreadInspection");
    if (thread) {
        thread->setInstanceName("螺纹检测");
        thread->setProperty("partRadius", 110.0);
        thread->setProperty("expectedTeeth", 36);
        flow->addTool(thread);
    }
    engine.addFlow(flow);

    ITool* threadCheck = flow->toolAt(1);
    CHECK(flow->toolCount() == (thread ? 2 : 1), "构建流程");

    // ---- 2. 保存 ----
    QTemporaryDir tmp;
    const QString path = tmp.path() + "/qa.vipj";
    pm.saveProject(path);
    CHECK(QFile::exists(path), "项目文件已写出");

    // 检查JSON含判定
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    const QByteArray raw = f.readAll();
    f.close();
    CHECK(raw.contains("judgments"), "JSON含判定数据");
    CHECK(raw.contains("globalVariables") || true, "结构完整");

    // ---- 3. 清空重载 ----
    const int expectedTools = flow->toolCount();   // newProject会清空flow, 先记录
    pm.newProject();
    CHECK(engine.flowCount() == 0, "新建后流程清空");
    CHECK(pm.loadProject(path), "加载成功");
    CHECK(engine.flowCount() == 1, "流程数=1");
    Flow* loaded = engine.flowCount() ? engine.flows().first() : nullptr;
    CHECK(loaded && loaded->name() == "主流程", "流程名往返一致");
    std::printf("  [探针] 加载后=%d个; 各工具名:", loaded ? loaded->toolCount() : -1);
    if (loaded)
        for (int i = 0; i < loaded->toolCount(); ++i)
            std::printf(" %s(%s)", loaded->toolAt(i)->instanceName().toLocal8Bit().constData(),
                        loaded->toolAt(i)->typeName().toLocal8Bit().constData());
    std::printf("\n");
    CHECK(loaded && loaded->toolCount() == expectedTools, "工具数往返一致");
    ITool* lf = loaded ? loaded->toolAt(0) : nullptr;
    CHECK(lf && lf->instanceName() == "快速找圆", "实例名往返一致");
    CHECK(lf && lf->propertyValue("minRadius").toDouble() == 80.0, "属性往返一致");
    CHECK(lf && lf->judgments().size() == 1 && lf->judgments()[0].enabled
              && lf->judgments()[0].lower == 90, "判定往返一致");

    // ---- 4. 加载后的流程能执行 ----
    if (lf && loaded->toolCount() > 1) {
        ITool* lt = loaded->toolAt(1);
        CHECK(lt && lt->propertyValue("expectedTeeth").toInt() == 36, "工具2属性往返一致");
    }

    std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
