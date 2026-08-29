/**
 * @file test_engine.cpp
 * @brief FlowEngine 引擎回归测试: 线性执行 / 属性引用解析 / 异步执行 / 中止响应
 *
 * 无头运行, 全部通过返回0。
 */

#include "../../src/engine/FlowEngine.h"
#include "../../src/engine/ToolRegistry.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <QSemaphore>
#include <cstdio>

#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

using namespace VisionInspector;

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [OK] %s\n", msg); } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

// ---- 测试工具A: 产生结果数据 ----
class ToolSource : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return "ToolSource"; }
    QString displayName() const override { return "数据源"; }
    ToolCategory category() const override { return ToolCategory::System; }
    PropertyDefList propertyDefs() const override {
        return { PropertyDef::doubleProp("value", "输出值", 42) };
    }
    bool execute(ToolContext& context) override {
        setResultData("value", propertyValue("value").toDouble());
#ifdef VI_HAS_OPENCV
        context.setCurrentImage(std::make_shared<CvImage>(64, 64, CV_8UC1, cv::Scalar(128)));
#endif
        return true;
    }
};

// ---- 测试工具B: 通过引用读取A的结果 ----
class ToolChecker : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return "ToolChecker"; }
    QString displayName() const override { return "校验器"; }
    ToolCategory category() const override { return ToolCategory::System; }
    PropertyDefList propertyDefs() const override {
        return { PropertyDef::stringProp("expect", "期望值", "$(数据源.value)") };
    }
    bool execute(ToolContext& context) override {
        m_received = propertyValue("expect").toString();
        m_flatValue = context.getString("数据源.value");   // 扁平命名空间
        return m_received == "42";
    }
    QString m_received;
    QString m_flatValue;
};

#include "test_engine.moc"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    auto& reg = ToolRegistry::instance();
    reg.registerTool("ToolSource", {"ToolSource", "数据源", ToolCategory::System,
                                    []() { return new ToolSource(); }});
    reg.registerTool("ToolChecker", {"ToolChecker", "校验器", ToolCategory::System,
                                     []() { return new ToolChecker(); }});
    CHECK(reg.allMetaData().size() >= 2, "工具注册成功");

    FlowEngine engine;

    // ---- 测试1: 同步执行 + 数据流 ----
    std::printf("测试1: 同步执行 + 属性引用解析\n");
    Flow* flow = new Flow(&engine);
    flow->setName("测试流程");
    auto* src = static_cast<ToolSource*>(reg.createTool("ToolSource"));
    src->setInstanceName("数据源");
    auto* checker = static_cast<ToolChecker*>(reg.createTool("ToolChecker"));
    checker->setInstanceName("校验器");
    flow->addTool(src);
    flow->addTool(checker);
    engine.addFlow(flow);

    ToolContext ctx;
    const bool ok = engine.executeOnce(flow, ctx);
    CHECK(ok, "流程执行返回OK");
    CHECK(checker->m_received == "42", "引用 $(数据源.value) 解析为42");
    CHECK(checker->m_flatValue == "42", "扁平命名空间 数据源.value 可读");
    CHECK(engine.lastImage() != nullptr, "末帧图像已记录");

    // ---- 测试2: 异步执行 + 信号回达 ----
    std::printf("测试2: 异步执行 + flowExecuted信号\n");
    QSemaphore done;
    bool asyncOk = false;
    QObject::connect(&engine, &FlowEngine::flowExecuted, [&](Flow*, bool allOk) {
        asyncOk = allOk;
        done.release();
    });
    engine.executeOnceAsync(flow);
    // 跨线程信号经事件队列送达, 等待时需处理事件
    bool asyncDone = false;
    QElapsedTimer wait2; wait2.start();
    while (wait2.elapsed() < 3000) {
        QCoreApplication::processEvents();
        if (done.tryAcquire(1, 20)) { asyncDone = true; break; }
    }
    CHECK(asyncDone && asyncOk, "异步执行完成且OK");

    // ---- 测试3: 连续运行停止响应 < 500ms ----
    std::printf("测试3: 连续运行停止响应\n");
    QSemaphore runStarted;
    QSemaphore runStopped;
    QObject::connect(&engine, &FlowEngine::runStateChanged, [&](bool running) {
        if (running) runStarted.release();
        else         runStopped.release();
    });
    engine.startRunning(flow);
    CHECK(runStarted.tryAcquire(1, 3000), "连续运行已启动");
    QThread::msleep(120);
    QElapsedTimer timer;
    timer.start();
    engine.stopRunning();
    // 等待停止信号(先连接后停止, 避免竞态丢失)
    bool stoppedFast = false;
    while (timer.elapsed() < 2000) {
        QCoreApplication::processEvents();
        if (runStopped.tryAcquire(1, 20)) { stoppedFast = timer.elapsed() < 500; break; }
    }
    CHECK(stoppedFast, QString("停止响应 %1ms < 500ms").arg(timer.elapsed()).toLocal8Bit().constData());

    std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
