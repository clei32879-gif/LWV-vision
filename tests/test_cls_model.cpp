/**
 * @file test_cls_model.cpp
 * @brief 自训缺陷分类模型验证 — 在验证集上测准确率
 *
 * 数据: testdata/cls_dataset/val/<类别>/*.jpg  (真实缺陷分类图-长的20%)
 * 模型: testdata/runs/classify 下 train 文件夹的 weights/best.onnx (最新)
 * 指标: 整体Top-1准确率 + 每类准确率 + 良品误报率
 */

#include "../../src/ai/InferEngine.h"
#include <opencv2/imgcodecs.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <cstdio>

using namespace VisionInspector;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const QString appDir = QCoreApplication::applicationDirPath();

    // 找最新模型
    QString model;
    const QStringList candidates = {
        appDir + "/../../../testdata/runs/classify/train/weights/best.onnx",
        appDir + "/models/defect_cls.onnx",
        appDir + "/models/yolov8n.onnx",
    };
    for (const QString& c : candidates)
        if (QFileInfo::exists(model)) break; else model = c;
    for (const QString& c : candidates)
        if (QFileInfo::exists(c)) { model = c; break; }
    if (!QFileInfo::exists(model)) {
        std::printf("[跳过] 未找到分类模型\n");
        return 0;
    }
    std::printf("模型: %s\n", model.toLocal8Bit().constData());

    // 验证集目录
    QString valDir = appDir + "/../../../testdata/cls_dataset/val";
    if (!QDir(valDir).exists()) valDir = appDir + "/testdata/cls_dataset/val";
    if (!QDir(valDir).exists()) {
        std::printf("[跳过] 未找到验证集\n");
        return 0;
    }

    QString err;
    auto engine = InferEngine::acquire(model, &err);
    if (!engine) { std::printf("[FAIL] 模型加载: %s\n", err.toLocal8Bit().constData()); return 1; }
    if (engine->outputKind() != InferEngine::OutputKind::Classification) {
        std::printf("[跳过] 非分类模型\n");
        return 0;
    }

    int total = 0, correct = 0;
    int goodTotal = 0, goodWrong = 0;   // 良品被误判为缺陷 = 误杀
    QMap<QString, QPair<int, int>> perClass;  // 类别 -> (正确, 总数)

    QElapsedTimer timer;
    timer.start();
    for (const QString& cls : QDir(valDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        for (const QFileInfo& fi :
             QDir(valDir + "/" + cls).entryInfoList({"*.jpg", "*.png"}, QDir::Files)) {
            cv::Mat img = cv::imread(fi.absoluteFilePath().toLocal8Bit().toStdString(),
                                     cv::IMREAD_COLOR);
            if (img.empty()) continue;
            QList<QPair<QString, float>> results;
            if (!engine->classify(img, results, &err)) continue;
            const QString top = results.isEmpty() ? QString() : results[0].first;
            ++total;
            static bool firstPrinted = false;
            if (!firstPrinted) {
                firstPrinted = true;
                std::printf("样例(%s): top3 =", cls.toLocal8Bit().constData());
                for (int i = 0; i < results.size() && i < 3; ++i)
                    std::printf(" %s(%.3f)", results[i].first.toUtf8().constData(),
                                results[i].second);
                std::printf("\n");
            }
            auto& pc = perClass[cls];
            pc.second++;
            if (top == cls) { ++correct; ++pc.first; }
            if (cls == QStringLiteral("良品")) {
                ++goodTotal;
                if (top != cls) ++goodWrong;
            }
        }
    }
    const qint64 ms = timer.elapsed();

    std::printf("\n===== 分类模型验证 =====\n");
    for (auto it = perClass.begin(); it != perClass.end(); ++it)
        std::printf("  %-24s %d/%d (%.0f%%)\n", it.key().toLocal8Bit().constData(),
                    it.value().first, it.value().second,
                    it.value().second ? 100.0 * it.value().first / it.value().second : 0);
    std::printf("整体Top-1: %d/%d (%.1f%%) | 良品误杀 %d/%d | 平均%lldms/张\n",
                correct, total, total ? 100.0 * correct / total : 0,
                goodWrong, goodTotal, total ? ms / total : 0);
    return 0;
}
