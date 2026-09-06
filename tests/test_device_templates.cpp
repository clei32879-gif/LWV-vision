/** @file test_device_templates.cpp
 *  @brief 内置设备模板回归 (阶段6): 4类模板 工具注册齐全 + 无头构建成功 + 工具数正确
 */
#include "../../src/core/DeviceTemplates.h"
#include "../../src/engine/FlowEngine.h"
#include "../../src/engine/ToolRegistry.h"
#include <QCoreApplication>
#include <QSet>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;
static int g_checks = 0;
#define CHECK(cond, msg) do { \
    ++g_checks; \
    if (cond) { } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    auto& reg = ToolRegistry::instance();
    CHECK(DeviceTemplates::all().size() >= 4, "内置模板>=4个 (筛选/贴标/计数/木业)");

    FlowEngine engine;

    for (const DeviceTemplate& t : DeviceTemplates::all()) {
        // 1) 模板引用的工具类型必须全部已注册 (缺注册=模板失效)
        int missing = 0;
        for (const QString& typeName : t.tools)
            if (!reg.isRegistered(typeName)) ++missing;
        CHECK(missing == 0, QString("%1: 工具类型全部注册 (缺%2个)")
                                .arg(t.name).arg(missing).toUtf8().constData());

        // 2) 无头构建成功且工具数与定义一致
        const int before = engine.flowCount();
        int created = 0;
        CHECK(DeviceTemplates::build(t.id, &engine, &created),
              QString("%1: 构建成功").arg(t.name).toUtf8().constData());
        CHECK(engine.flowCount() == before + 1, QString("%1: 新增1条流程").arg(t.name).toUtf8().constData());
        CHECK(created == t.tools.size(), QString("%1: 工具数%2=%3")
              .arg(t.name).arg(created).arg(t.tools.size()).toUtf8().constData());

        // 3) 流程内实例名无重复 (重名自动 _N 生效)
        Flow* flow = engine.flows().last();
        QSet<QString> names;
        int dup = 0;
        for (int i = 0; i < flow->toolCount(); ++i) {
            const QString n = flow->toolAt(i)->instanceName();
            if (names.contains(n)) ++dup;
            else names.insert(n);
        }
        CHECK(dup == 0, QString("%1: 实例名无重复").arg(t.name).toUtf8().constData());

        // 4) byId 反查一致
        CHECK(DeviceTemplates::byId(t.id) == &t, QString("%1: byId命中").arg(t.name).toUtf8().constData());
    }

    // 未知id构建失败
    CHECK(!DeviceTemplates::build(QStringLiteral("__nope__"), &engine), "未知模板id构建失败");

    engine.shutdownAndWait(3000);

    std::printf("\n设备模板: %d项检查, 失败%d\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
