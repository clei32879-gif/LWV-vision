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
#include <atomic>
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

// ---- 测试工具C: 慢执行 (模拟耗时算法, 验证shutdownAndWait会等待) ----
class ToolSlow : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return "ToolSlow"; }
    QString displayName() const override { return "慢工具"; }
    ToolCategory category() const override { return ToolCategory::System; }
    PropertyDefList propertyDefs() const override {
        return { PropertyDef::intProp("sleepMs", "耗时(ms)", 200) };
    }
    bool execute(ToolContext&) override {
        QThread::msleep(propertyValue("sleepMs").toInt());
        return true;
    }
};

// ---- 测试工具D: H-4/H-5 并发写接口 (暴露 protected 写方法给压力线程) ----
class ToolStress : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return "ToolStress"; }
    QString displayName() const override { return "并发压力"; }
    ToolCategory category() const override { return ToolCategory::System; }
    PropertyDefList propertyDefs() const override {
        return { PropertyDef::doubleProp("value", "输出值", 1.0) };
    }
    bool execute(ToolContext&) override { return true; }
    // 压力线程: 模拟工作线程写属性/结果/状态
    void stressWrite(int seq) {
        setProperty("value", seq);
        setResultData("value", seq);
        setResultData("seq", seq);
        setStatus(seq % 2 ? ToolStatus::OK : ToolStatus::NG);
    }
};

// ---- 测试辅助: 执行计数器 (验证循环体实际执行次数) ----
class ToolCounter : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return "ToolCounter"; }
    QString displayName() const override { return "计数器"; }
    ToolCategory category() const override { return ToolCategory::Logic; }
    PropertyDefList propertyDefs() const override { return {}; }
    bool execute(ToolContext&) override {
        ++m_count;
        setStatus(ToolStatus::OK);
        return true;
    }
    int m_count = 0;
};

#include "test_engine.moc"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    auto& reg = ToolRegistry::instance();
    reg.registerTool("ToolSource", {"ToolSource", "数据源", ToolCategory::System,
                                    []() { return new ToolSource(); }});
    reg.registerTool("ToolChecker", {"ToolChecker", "校验器", ToolCategory::System,
                                     []() { return new ToolChecker(); }});
    reg.registerTool("ToolSlow", {"ToolSlow", "慢工具", ToolCategory::System,
                                  []() { return new ToolSlow(); }});
    reg.registerTool("ToolStress", {"ToolStress", "并发压力", ToolCategory::System,
                                    []() { return new ToolStress(); }});
    reg.registerTool("ToolCounter", {"ToolCounter", "计数器", ToolCategory::Logic,
                                     []() { return new ToolCounter(); }});
    CHECK(reg.allMetaData().size() >= 3, "工具注册成功");

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

    // ---- 测试4: shutdownAndWait 真正等待工作线程结束 ----
    std::printf("测试4: shutdownAndWait 等待工作线程结束\n");
    {
        Flow* slowFlow = new Flow(&engine);
        slowFlow->setName("慢流程");
        ITool* slow = reg.createTool("ToolSlow");
        slow->setInstanceName("慢工具");
        slow->setProperty("sleepMs", 200);
        slowFlow->addTool(slow);
        engine.addFlow(slowFlow);

        // 连续运行 (每轮200ms慢工具), 然后立刻 shutdownAndWait 应阻塞至当前轮结束
        engine.startRunning(slowFlow);
        QThread::msleep(50);   // 让工作线程进入慢工具执行
        QElapsedTimer sw;
        sw.start();
        engine.shutdownAndWait(3000);
        const qint64 waited = sw.elapsed();
        CHECK(waited >= 150, QString("shutdownAndWait 等待慢工具执行完: %1ms").arg(waited)
                                 .toLocal8Bit().constData());
        CHECK(!engine.isRunning(), "shutdownAndWait 后不再运行");
        CHECK(!engine.isExecuting(), "shutdownAndWait 后不在执行");
        engine.removeFlow(slowFlow);
        slowFlow->deleteLater();
    }

    // ---- 测试5: Flow 拥有工具, 删除Flow后工具被回收 (H-1) ----
    std::printf("测试5: Flow 工具所有权/回收\n");
    {
        Flow* f2 = new Flow(&engine);
        f2->setName("所有权流程");
        ITool* a = reg.createTool("ToolSource");
        a->setInstanceName("数据源A");
        ITool* b = reg.createTool("ToolChecker");
        b->setInstanceName("校验器B");
        f2->addTool(a);
        f2->addTool(b);
        engine.addFlow(f2);
        CHECK(f2->toolCount() == 2, "流程含2工具");
        engine.removeFlow(f2);
        f2->setParent(nullptr);
        f2->deleteLater();   // 析构时 clear() 删除工具
        QCoreApplication::processEvents();   // 触发 deleteLater
        CHECK(!engine.flows().contains(f2), "流程已移除");
        // 工具指针已由Flow回收, 此处仅验证不崩溃且计数正确
        CHECK(true, "删除流程(含工具回收)未崩溃");
    }

    // ---- 测试6: H-4/H-5 属性/结果/状态并发读写 (工作线程写 vs 主线程读) ----
    std::printf("测试6: 工具状态并发读写压力\n");
    {
        auto* stress = static_cast<ToolStress*>(reg.createTool("ToolStress"));
        stress->setInstanceName("压力工具");
        std::atomic<bool> stop{false};
        std::atomic<int> writes{0};
        // 工作线程: 高频写属性/结果/状态
        auto writer = QtConcurrent::run([stress, &stop, &writes] {
            int seq = 0;
            while (!stop.load(std::memory_order_relaxed)) {
                stress->stressWrite(seq++);
                writes.store(seq, std::memory_order_relaxed);
            }
        });
        // 主线程: 高频读属性/结果/状态 (与写并发, 应无崩溃/无半写状态)
        // 注意: 单次 resultData() 是锁内整表快照副本;
        // 多个键是分次锁写, 跨键可能读到合法中间态(值5+序4), 属正常语义, 不做跨键断言
        QThread::msleep(20);   // 先让写线程启动, 避免首读遇空快照误判
        QElapsedTimer timer;
        timer.start();
        int reads = 0;
        bool snapshotIntact = true;
        while (timer.elapsed() < 400) {
            (void)stress->propertyValue("value");
            const DataMap rd = stress->resultData();      // 锁内快照副本
            // 单键值必须是有效整数(无半写/撕裂的 QVariant); 空快照容忍
            if (!rd.isEmpty()) {
                bool okNum = false;
                const int v = rd.value("value").toInt(&okNum);
                if (!okNum || v < 0) { snapshotIntact = false; break; }
            }
            (void)stress->status();
            ++reads;
            if (reads % 5000 == 0) QThread::msleep(1);
        }
        stop.store(true, std::memory_order_relaxed);
        writer.waitForFinished();
        CHECK(reads > 0, "主线程高频读取完成");
        CHECK(snapshotIntact, "结果快照单键值完整(未读半写状态)");
        CHECK(writes.load() > 0, "工作线程高频写入完成");
        delete stress;
    }

    // ---- 测试7: 循环回跳 (M-23 修复): Loop/LoopEnd 有限循环 ----
    std::printf("测试7: 循环回跳 Loop/LoopEnd\n");
    {
        Flow* lf = new Flow(&engine);
        lf->setName("循环流程");
        ITool* loop = reg.createTool("Loop");
        loop->setInstanceName("循环");
        loop->setProperty("loopMode", 0);       // 递增 A→B-1
        loop->setProperty("startValue", 1);
        loop->setProperty("endValue", 4);
        auto* cnt = static_cast<ToolCounter*>(reg.createTool("ToolCounter"));
        cnt->setInstanceName("计数");
        ITool* lend = reg.createTool("LoopEnd");
        lend->setInstanceName("循环结束");
        lf->addTool(loop);
        lf->addTool(cnt);
        lf->addTool(lend);
        engine.addFlow(lf);

        ToolContext ctx7;
        const bool ok7 = engine.executeOnce(lf, ctx7);
        CHECK(ok7, "循环流程执行返回OK");
        CHECK(cnt->m_count == 3, QString("循环体执行3次(实际%1)")
                                    .arg(cnt->m_count).toLocal8Bit().constData());
        CHECK(ctx7.getInt("loopIndex", -1) == 3, "循环索引最终=3 (从1到B-1=3)");
        CHECK(ctx7.getBool("__loop_active", true) == false, "循环结束后活动标志清除");

        engine.removeFlow(lf);
        lf->setParent(nullptr);
        lf->deleteLater();
        QCoreApplication::processEvents();
    }

    // ---- 测试8: 停止循环 (P0-9 补齐): 无限循环 + 数据条件停止 ----
    std::printf("测试8: 停止循环 StopLoop (数据满足)\n");
    {
        Flow* sf = new Flow(&engine);
        sf->setName("停止循环流程");
        ITool* loop = reg.createTool("Loop");
        loop->setInstanceName("循环");
        loop->setProperty("loopMode", 2);       // 无限
        ITool* stop = reg.createTool("StopLoop");
        stop->setInstanceName("停止");
        stop->setProperty("stopWhen", 2);       // 数据满足
        stop->setProperty("sourceDataKey", "loopIndex");
        stop->setProperty("expression", ">= 3");
        auto* cnt = static_cast<ToolCounter*>(reg.createTool("ToolCounter"));
        cnt->setInstanceName("计数");
        ITool* lend = reg.createTool("LoopEnd");
        lend->setInstanceName("循环结束");
        sf->addTool(loop);
        sf->addTool(stop);
        sf->addTool(cnt);
        sf->addTool(lend);
        engine.addFlow(sf);

        ToolContext ctx8;
        const bool ok8 = engine.executeOnce(sf, ctx8);
        CHECK(ok8, "停止循环流程执行返回OK");
        CHECK(cnt->m_count == 2, QString("StopLoop(loopIndex>=3)触发后跳过计数(实际%1)")
                                    .arg(cnt->m_count).toLocal8Bit().constData());
        CHECK(ctx8.getBool("__loop_active", true) == false, "停止后活动标志清除");

        engine.removeFlow(sf);
        sf->setParent(nullptr);
        sf->deleteLater();
        QCoreApplication::processEvents();
    }

    // ---- 测试9: 停止循环兜底路径: 无条件停止 + 引擎未记录循环结束索引 ----
    std::printf("测试9: 停止循环 StopLoop (无条件/兜底)\n");
    {
        Flow* uf = new Flow(&engine);
        uf->setName("无条件停止流程");
        ITool* loop = reg.createTool("Loop");
        loop->setInstanceName("循环");
        loop->setProperty("loopMode", 2);       // 无限
        ITool* stop = reg.createTool("StopLoop");
        stop->setInstanceName("停止");
        stop->setProperty("stopWhen", 0);       // 无条件
        auto* cnt = static_cast<ToolCounter*>(reg.createTool("ToolCounter"));
        cnt->setInstanceName("计数");
        ITool* lend = reg.createTool("LoopEnd");
        lend->setInstanceName("循环结束");
        uf->addTool(loop);
        uf->addTool(stop);
        uf->addTool(cnt);
        uf->addTool(lend);
        engine.addFlow(uf);

        ToolContext ctx9;
        const bool ok9 = engine.executeOnce(uf, ctx9);
        CHECK(ok9, "无条件停止流程执行返回OK");
        CHECK(cnt->m_count == 1, QString("无条件停止后循环体仅执行1次(实际%1)")
                                    .arg(cnt->m_count).toLocal8Bit().constData());
        CHECK(ctx9.getBool("__loop_active", true) == false, "兜底路径循环状态已清除");

        engine.removeFlow(uf);
        uf->setParent(nullptr);
        uf->deleteLater();
        QCoreApplication::processEvents();
    }

    std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
