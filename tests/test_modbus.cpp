/**
 * @file test_modbus.cpp
 * @brief Modbus TCP/RTU 主站-从站模拟器联测 (无头运行)
 *
 * 流程:
 *   - TCP: 后台线程用主站读写, 主线程跑事件循环驱动 TCP 从站。
 *   - RTU: 用 QLocalServer/QLocalSocket 对充当"虚拟串口线", 主线程驱动
 *          RTU 从站(事件循环), 后台线程用 RTU 主站(阻塞读写)。
 * 全部断言通过返回0, 任何失败返回1。
 */

#include "../../src/hal/ModbusTcpMaster.h"
#include "../../src/hal/ModbusTcpSlave.h"
#include "../../src/hal/ModbusRtuMaster.h"
#include "../../src/hal/ModbusRtuSlave.h"
#include "../../src/hal/ModbusIoDriver.h"
#include "../../src/hal/ModbusPLCDriver.h"

#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <QSemaphore>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <atomic>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;

#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [OK] %s\n", msg); } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

/** RTU 主站测试体 (在后台线程执行; 客户端socket在本线程创建/使用) */
static void rtuMasterWorker(const QString& pipeName, std::atomic<bool>* serverReady,
                            int slaveId, QSemaphore* done) {
    // 在本线程创建并连接"虚拟串口线"客户端端
    QLocalSocket client;
    client.connectToServer(pipeName);
    if (!client.waitForConnected(3000)) {
        std::printf("[FATAL] RTU客户端连接失败: %s\n",
                    client.errorString().toLocal8Bit().constData());
        ++g_failures;
        done->release();
        return;
    }
    // 等服务端(主线程)已接受连接并接好从站
    for (int i = 0; i < 100 && !serverReady->load(); ++i)
        QThread::msleep(20);
    if (!serverReady->load()) {
        std::printf("[FATAL] RTU服务端未就绪\n");
        ++g_failures;
        done->release();
        return;
    }

    QString e;
    ModbusRtuMaster master(ModbusRtuMaster::SerialParams{});
    master.setDeviceForTest(&client);

    std::printf("RTU测试1: 写单寄存器/读保持寄存器\n");
    CHECK(master.writeRegister(10, 777, slaveId, &e), "写 R10=777");
    e.clear();
    auto regs = master.readRegisters(3, 10, 1, slaveId, &e);
    CHECK(regs.size() == 1 && regs[0] == 777, "读回 R10==777");

    std::printf("RTU测试2: 写多寄存器/读回\n");
    QVector<quint16> vals = {11, 22, 33};
    CHECK(master.writeRegisters(20, vals, slaveId, &e), "写 R20..R22");
    e.clear();
    regs = master.readRegisters(3, 20, 3, slaveId, &e);
    CHECK(regs == vals, "读回 R20..R22 == 11,22,33");

    std::printf("RTU测试3: 写线圈/读线圈\n");
    CHECK(master.writeCoil(5, true, slaveId, &e), "写 C5=ON");
    e.clear();
    auto bits = master.readBits(1, 5, 1, slaveId, &e);
    CHECK(bits.size() == 1 && bits[0], "读回 C5==ON");

    std::printf("RTU测试4: 输入寄存器映射保持寄存器\n");
    master.writeRegister(30, 555, slaveId, &e);
    e.clear();
    regs = master.readRegisters(4, 30, 1, slaveId, &e);
    CHECK(regs.size() == 1 && regs[0] == 555, "输入寄存器 IR30==555");

    std::printf("RTU测试5: 非法地址异常响应\n");
    regs = master.readRegisters(3, 65530, 10, slaveId, &e);
    CHECK(regs.isEmpty() && e.contains("异常"), "收到异常码且不崩溃");

    std::printf("RTU测试6: 契约高地址读写 (模拟信捷XD5 41088+D)\n");
    // 契约: D6命令=41094, OK产量=41258
    CHECK(master.writeRegister(41094, 1, slaveId, &e), "写 D6命令=1(启动)");
    e.clear();
    regs = master.readRegisters(3, 41258, 1, slaveId, &e);
    CHECK(regs.size() == 1 && regs[0] == 0, "读 OK产量初值0");
    CHECK(master.writeRegister(41258, 25, slaveId, &e), "写 OK产量=25");
    e.clear();
    regs = master.readRegisters(3, 41258, 1, slaveId, &e);
    CHECK(regs.size() == 1 && regs[0] == 25, "读回 OK产量==25");

    std::printf("RTU测试7: 写多线圈 FC15\n");
    QVector<bool> coils = {true, false, true, true};
    CHECK(master.writeCoils(100, coils, slaveId, &e), "写 C100..C103");
    e.clear();
    bits = master.readBits(1, 100, 4, slaveId, &e);
    CHECK(bits == coils, "读回 C100..C103");

    done->release();
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, nullptr, _IONBF, 0);   // 行缓冲, 便于观察进度
    std::printf("TEST START\n");
    fflush(stdout);
    QCoreApplication app(argc, argv);

    // ===================== TCP 联测 =====================
    std::printf("===== TCP 主站/从站联测 =====\n");
    ModbusTcpSlave slave;
    QString err;
    const quint16 port = 15020;
    if (!slave.start("127.0.0.1", port, &err)) {
        std::printf("[FATAL] TCP从站启动失败: %s\n", err.toLocal8Bit().constData());
        return 1;
    }
    std::printf("TCP从站已启动 127.0.0.1:%u\n", unsigned(port));

    QSemaphore doneTcp;
    QtConcurrent::run([&]() {
        QString e;
        auto* m = ModbusTcpMaster::acquire("127.0.0.1", port, &e);

        std::printf("TCP测试1: 写单寄存器/读保持寄存器\n");
        CHECK(m->writeRegister(10, 777, 1, &e), "写 R10=777");
        auto regs = m->readRegisters(3, 10, 1, 1, &e);
        CHECK(regs.size() == 1 && regs[0] == 777, "读回 R10==777");

        std::printf("TCP测试2: 写多寄存器/读回\n");
        QVector<quint16> vals = {11, 22, 33};
        const bool wok = m->writeRegisters(20, vals, 1, &e);
        if (!wok) std::printf("  [调试] writeRegisters错误: %s\n", e.toLocal8Bit().constData());
        CHECK(wok, "写 R20..R22");
        e.clear();
        regs = m->readRegisters(3, 20, 3, 1, &e);
        CHECK(regs == vals, "读回 R20..R22 == 11,22,33");

        std::printf("TCP测试3: 写线圈/读线圈\n");
        CHECK(m->writeCoil(5, true, 1, &e), "写 C5=ON");
        auto bits = m->readBits(1, 5, 1, 1, &e);
        CHECK(bits.size() == 1 && bits[0], "读回 C5==ON");

        std::printf("TCP测试4: 输入寄存器映射保持寄存器\n");
        m->writeRegister(30, 555, 1, &e);
        regs = m->readRegisters(4, 30, 1, 1, &e);
        CHECK(regs.size() == 1 && regs[0] == 555, "输入寄存器 IR30==555");

        std::printf("TCP测试5: 非法地址异常响应\n");
        regs = m->readRegisters(3, 65530, 10, 1, &e);
        CHECK(regs.isEmpty() && e.contains("异常"), "收到异常码且不崩溃");

        std::printf("TCP测试6: 从站写入信号\n");
        quint16 written = 0;
        QObject::connect(&slave, &ModbusTcpSlave::registerWritten,
                         [&](int, quint16 v) { written = v; });
        m->writeRegister(40, 888, 1, &e);
        QThread::msleep(100);
        QCoreApplication::processEvents();
        CHECK(written == 888, "registerWritten信号收到888");

        std::printf("TCP测试7: ModbusIoDriver 读写 (N1616替代通道)\n");
        {
            ModbusIoDriver io;
            CHECK(io.connect("transport=tcp;host=127.0.0.1;port=15020;slaveId=1;"
                             "coilOut=1;outBase=50;inBase=60;readInputAsCoil=1;"
                             "inputCount=8;outputCount=8"),
                  "IO驱动连接(TCP)");
            CHECK(io.outputCount() == 8 && io.inputCount() == 8, "IO通道数为8/8");

            // 写输出 → PLC线圈 M50(col地址50)
            CHECK(io.writeOutput(0, true), "IO写输出 ch0=ON");
            QThread::msleep(50);
            QCoreApplication::processEvents();
            CHECK(slave.coil(50) == true, "从站线圈M50已置ON");

            // 读输入 ← PLC线圈 M60
            slave.setCoil(60, true);
            bool v = false;
            CHECK(io.readInput(0, v) && v, "IO读输入 ch0==ON(M60置位)");
            slave.setCoil(60, false);
            v = true;
            CHECK(io.readInput(0, v) && !v, "IO读输入 ch0==OFF(M60复位)");

            // 脉冲输出: 同步置ON→延时→复位
            CHECK(io.pulseOutput(0, 80), "IO脉冲输出 ch0 80ms");
            CHECK(slave.coil(50) == false, "脉冲结束后M50自动复位");
        }

        std::printf("TCP测试8: ModbusPLCDriver 业务契约 (信捷XD5筛选机)\n");
        {
            ModbusPLCDriver plc;
            PLCConnectionParams p;
            p.commType = PLCCommType::ModbusTCP;
            p.ipAddress = "127.0.0.1";
            p.port = port;
            p.slaveId = 1;
            CHECK(plc.connect(p), "PLC驱动连接(TCP)");
            CHECK(plc.isConnected(), "PLC驱动已连接");
            CHECK(plc.driverName().contains("XD"), "驱动名含XD");

            // 契约: D6命令=41094, HD170 OK产量=41258, HD0 手动速度=41088
            CHECK(plc.sendCommand(1), "写 D6=1(启动命令)");
            QThread::msleep(30);
            CHECK(slave.holding(41094) == 1, "从站D6==1");

            CHECK(plc.setManualSpeed(800), "写 HD0=800 手动速度");
            QThread::msleep(30);
            CHECK(slave.holding(41088) == 800, "从站HD0==800");

            // 产量统计写入+回读 (HD170=25)
            CHECK(plc.writeHoldingRegisters(41258, QVector<quint16>{25}), "写 HD170=25");
            QThread::msleep(30);
            CHECK(plc.readOKCount() == 25, "readOKCount()==25");
            QVector<quint16> hr;
            CHECK(plc.readHoldingRegisters(41258, 1, hr) && hr.size() == 1 && hr[0] == 25,
                  "读保持寄存器 HD170==25");

            // 伺服正转线圈 M50
            CHECK(plc.servoForward(true), "写 M50=ON 伺服正转");
            QThread::msleep(30);
            CHECK(slave.coil(50) == true, "从站M50==ON");
            CHECK(plc.servoForward(false), "写 M50=OFF");
            QThread::msleep(30);
            CHECK(slave.coil(50) == false, "从站M50==OFF");

            // OK吹气: M106 置ON→复位
            CHECK(plc.blowOK(40), "blowOK(40ms)");
            QThread::msleep(30);
            CHECK(slave.coil(106) == false, "吹气结束后M106复位");

            // 手动拍照 M121 (相机1)
            CHECK(plc.manualShot(1), "手动拍照 M121=ON");
            QThread::msleep(30);
            CHECK(slave.coil(121) == true, "从站M121==ON");
        }

        doneTcp.release();
    });

    // ===================== RTU 联测 (虚拟串口线) =====================
    std::printf("\n===== RTU 主站/从站联测 (QLocalSocket虚拟串口) =====\n");
    const QString pipeName = QStringLiteral("lwv-rtu-test-%1").arg(QCoreApplication::applicationPid());
    QLocalServer server;
    QLocalServer::removeServer(pipeName);
    QPointer<ModbusRtuSlave> rtuSlave = new ModbusRtuSlave();
    std::atomic<bool> serverReady{false};

    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        rtuSlave->setDeviceForTest(server.nextPendingConnection());
        rtuSlave->setAutoIncrement(-1);   // 关闭自增, 便于确定性断言
        serverReady = true;
    });
    if (!server.listen(pipeName)) {
        std::printf("[FATAL] QLocalServer监听失败: %s\n",
                    server.errorString().toLocal8Bit().constData());
        return 1;
    }

    QSemaphore doneRtu;
    QtConcurrent::run([&, pipeName]() {
        rtuMasterWorker(pipeName, &serverReady, 1, &doneRtu);
    });

    // ===================== 主线程事件循环 =====================
    QTimer finishTimer;
    finishTimer.setSingleShot(true);
    QObject::connect(&finishTimer, &QTimer::timeout, &app, [&]() {
        // available() 不消耗信号量 (tryAcquire 会吃掉导致误判)
        const int tcpAvail = doneTcp.available();
        const int rtuAvail = doneRtu.available();
        std::printf("  [finish] tcp=%d rtu=%d\n", tcpAvail, rtuAvail);
        fflush(stdout);
        if (tcpAvail > 0 && rtuAvail > 0) {
            slave.stop();
            ModbusTcpMaster::releaseAll();
            ModbusRtuMaster::releaseAll();
            if (rtuSlave) rtuSlave->stop();
            server.close();
            std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
            fflush(stdout);
            app.exit(g_failures == 0 ? 0 : 1);
        } else {
            finishTimer.start(200);
        }
    });
    finishTimer.start(200);

    return app.exec();
}
