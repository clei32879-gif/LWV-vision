/**
 * @file test_teach_flow.cpp
 * @brief QA: 教导向导核心链路端到端测试 (无GUI部分)
 *
 * 模拟用户流程:
 *   1. 用真实样本图模拟"抓帧" — 拷贝到 teach/上视/OK 与 /NG
 *   2. prepareDataset 逻辑 (80/20切分)
 *   3. train_cls.py 真实训练 (小轮数快速验证)
 *   4. 模型产出 → InferEngine 加载 → 分类推理
 * 注: 训练环节用10轮快速模式, 全流程预计2-5分钟
 */

#include "../../src/ai/InferEngine.h"
#include <opencv2/imgcodecs.hpp>
#include <QCoreApplication>
#include <QProcess>
#include <QDir>
#include <QElapsedTimer>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [OK] %s\n", msg); } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString root = appDir + "/../../../";
    const QString teachDir = root + "teach/上视";
    const QString script = root + "tools/train_cls.py";

    // ---- 1. 模拟抓帧: 真实样本图 → teach/上视/{OK,NG} ----
    std::printf("步骤1: 模拟抓帧 (良品→OK, 头麻→NG)\n");
    QDir(teachDir).removeRecursively();   // 干净起点
    int okCnt = 0, ngCnt = 0;
    for (const char* clsDir : {"良品"}) {
        QDir srcDir(QString("%1/testdata/cls_dataset/train/%2").arg(root, clsDir));
        for (const QFileInfo& fi : srcDir.entryInfoList({"*.jpg"}, QDir::Files)) {
            if (okCnt >= 12) break;   // 模拟抓12帧
            QDir(teachDir + "/OK").mkpath(".");
            QFile::copy(fi.absoluteFilePath(),
                        teachDir + "/OK/OK_" + fi.fileName());
            ++okCnt;
        }
    }
    QDir ngDir(QString("%1/testdata/cls_dataset/train/头麻").arg(root));
    for (const QFileInfo& fi : ngDir.entryInfoList({"*.jpg"}, QDir::Files)) {
        if (ngCnt >= 12) break;
        QDir(teachDir + "/NG").mkpath(".");
        QFile::copy(fi.absoluteFilePath(),
                    teachDir + "/NG/NG_" + fi.fileName());
        ++ngCnt;
    }
    CHECK(okCnt >= 10, QString("OK样本抓取 %1 张").arg(okCnt).toLocal8Bit().constData());
    CHECK(ngCnt >= 10, QString("NG样本抓取 %1 张").arg(ngCnt).toLocal8Bit().constData());

    // ---- 2. 数据集切分 (向导的prepareDataset逻辑) ----
    std::printf("步骤2: 数据集切分 (80/20)\n");
    const QString dataDir = teachDir + "/train_data";
    QDir(dataDir).removeRecursively();
    int tr = 0, va = 0, i = 0;
    for (const QString& sub : {QStringLiteral("OK"), QStringLiteral("NG")}) {
        const QString cls = (sub == "OK") ? QStringLiteral("良品") : QStringLiteral("缺陷");
        QDir dir(teachDir + "/" + sub);
        for (const QFileInfo& fi : dir.entryInfoList({"*.jpg", "*.png"}, QDir::Files, QDir::Time)) {
            const QString split = (i++ % 5 == 4) ? "val" : "train";
            QDir(dataDir + "/" + split + "/" + cls).mkpath(".");
            QFile::copy(fi.absoluteFilePath(),
                        dataDir + "/" + split + "/" + cls + "/" + fi.fileName());
            (split == "train") ? ++tr : ++va;
        }
    }
    QFile cf(dataDir + "/classes.txt");
    if (cf.open(QIODevice::WriteOnly)) { cf.write("良品\n缺陷\n"); cf.close(); }
    CHECK(tr >= 15 && va >= 3, QString("切分: 训练%1 验证%2").arg(tr).arg(va).toLocal8Bit().constData());

    // ---- 3. 真实训练 (10轮快速) ----
    std::printf("步骤3: 调用 train_cls.py 真实训练 (10轮快速验证)\n");
    if (!QFile::exists(script)) {
        std::printf("[FAIL] 训练脚本缺失: %s\n", script.toLocal8Bit().constData());
        return 1;
    }
    const QString outModel = root + "teach/上视.onnx";
    QElapsedTimer timer;
    timer.start();
    QProcess proc;
    proc.setWorkingDirectory(appDir);
    proc.start("python", {script, QDir::toNativeSeparators(dataDir),
                          QDir::toNativeSeparators(outModel), "10", "cpu"});
    if (!proc.waitForStarted(5000)) {
        std::printf("[FAIL] Python进程无法启动 (本机需装ultralytics)\n");
        return 1;
    }
    proc.waitForFinished(-1);
    const qint64 trainMs = timer.elapsed();
    CHECK(proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0,
          QString("训练完成 (%1分钟) 输出: %2")
              .arg(trainMs / 60000.0, 0, 'f', 1)
              .arg(QString(proc.readAllStandardOutput()).right(80))
              .toLocal8Bit().constData());
    CHECK(QFile::exists(outModel), "ONNX模型产出");

    // ---- 4. C++加载训练出的模型并推理 ----
    std::printf("步骤4: C++加载自训模型推理\n");
    QString err;
    auto eng = InferEngine::acquire(outModel, &err);
    CHECK(eng != nullptr, QString("模型加载: %1").arg(err).toLocal8Bit().constData());
    if (eng) {
        CHECK(eng->outputKind() == InferEngine::OutputKind::Classification,
              "识别为分类模型");
        // 用一张训练集良品图推理 — top1应为良品
        QDir goodDir(dataDir + "/train/良品");
        const auto files = goodDir.entryList({"*.jpg"}, QDir::Files);
        if (!files.isEmpty()) {
            cv::Mat img = cv::imread(
                goodDir.absoluteFilePath(files[0]).toLocal8Bit().toStdString(),
                cv::IMREAD_COLOR);
            QList<QPair<QString, float>> res;
            eng->classify(img, res, &err);
            CHECK(!res.isEmpty() && res[0].first == QStringLiteral("良品"),
                  QString("良品图推理 -> %1(%2)")
                      .arg(res.value(0).first)
                      .arg(res.value(0).second, 0, 'f', 2).toLocal8Bit().constData());
        }
    }

    std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "教导全流程通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
