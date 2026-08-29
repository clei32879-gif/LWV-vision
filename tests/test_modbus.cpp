/**
 * @file test_modbus.cpp
 * @brief Modbus TCP 主站/从站模拟器联测 (无头运行)
 *
 * 流程: 后台线程用主站读写, 主线程跑事件循环驱动从站。
 * 全部断言通过返回0, 任何失败返回1。
 */

#include "../../src/hal/ModbusTcpMaster.h"
#include "../../src/hal/ModbusTcpSlave.h"

#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <QSemaphore>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;

#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [OK] %s\n", msg); } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    ModbusTcpSlave slave;
    QString err;
    const quint16 port = 15020;
    if (!slave.start("127.0.0.1", port, &err)) {
        std::printf("[FATAL] 从站启动失败: %s\n", err.toLocal8Bit().constData());
        return 1;
    }
    std::printf("从站已启动 127.0.0.1:%u\n", unsigned(port));

    QSemaphore done;
    QtConcurrent::run([&]() {
        QString e;
        auto* m = ModbusTcpMaster::acquire("127.0.0.1", port, &e);

        std::printf("测试1: 写单寄存器/读保持寄存器\n");
        CHECK(m->writeRegister(10, 777, 1, &e), "写 R10=777");
        auto regs = m->readRegisters(3, 10, 1, 1, &e);
        CHECK(regs.size() == 1 && regs[0] == 777, "读回 R10==777");

        std::printf("测试2: 写多寄存器/读回\n");
        QVector<quint16> vals = {11, 22, 33};
        const bool wok = m->writeRegisters(20, vals, 1, &e);
        if (!wok) std::printf("  [调试] writeRegisters错误: %s\n", e.toLocal8Bit().constData());
        CHECK(wok, "写 R20..R22");
        e.clear();
        regs = m->readRegisters(3, 20, 3, 1, &e);
        if (regs != vals) std::printf("  [调试] 读回错误: %s\n", e.toLocal8Bit().constData());
        CHECK(regs == vals, "读回 R20..R22 == 11,22,33");

        std::printf("测试3: 写线圈/读线圈\n");
        CHECK(m->writeCoil(5, true, 1, &e), "写 C5=ON");
        auto bits = m->readBits(1, 5, 1, 1, &e);
        CHECK(bits.size() == 1 && bits[0], "读回 C5==ON");

        std::printf("测试4: 输入寄存器映射保持寄存器\n");
        m->writeRegister(30, 555, 1, &e);
        regs = m->readRegisters(4, 30, 1, 1, &e);
        CHECK(regs.size() == 1 && regs[0] == 555, "输入寄存器 IR30==555");

        std::printf("测试5: 非法地址异常响应\n");
        regs = m->readRegisters(3, 6000, 10, 1, &e);
        CHECK(regs.isEmpty() && e.contains("异常"), "收到异常码且不崩溃");

        std::printf("测试6: 从站写入信号\n");
        quint16 written = 0;
        QObject::connect(&slave, &ModbusTcpSlave::registerWritten,
                         [&](int, quint16 v) { written = v; });
        m->writeRegister(40, 888, 1, &e);
        // 信号直连同线程队列 — 等一拍
        QThread::msleep(100);
        QCoreApplication::processEvents();
        CHECK(written == 888, "registerWritten信号收到888");

        done.release();
    });

    // 从站信号直连需在主线程处理事件
    QTimer finishTimer;
    finishTimer.setSingleShot(true);
    QObject::connect(&finishTimer, &QTimer::timeout, &app, [&]() {
        if (done.tryAcquire()) {
            slave.stop();
            ModbusTcpMaster::releaseAll();
            std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
            app.exit(g_failures == 0 ? 0 : 1);
        } else {
            finishTimer.start(200);
        }
    });
    finishTimer.start(200);

    return app.exec();
}
