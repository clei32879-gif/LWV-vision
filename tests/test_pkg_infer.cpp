/**
 * @file test_pkg_infer.cpp
 * @brief 发布包环境 AI 真实推理验证 (质检2)
 *
 * 从发布包目录(默认 CWD, 亦可 argv[1] 指定)读取:
 *   models/defect_cls.onnx  (分类)  + testdata/real_samples/*.png
 *   models/yolov8n.onnx     (检测)  + testdata/real_samples/*.png
 * 逐张真实加载模型并推理 (非冒烟: 同级 onnxruntime.dll 真实执行),
 * 结果写入 <root>/qc_ai_infer_result.txt (UTF-8)。
 */
#include "../../src/ai/InferEngine.h"
#include <opencv2/imgcodecs.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QElapsedTimer>
#include <cstdio>

using namespace VisionInspector;

static QString norm(const QString& p)
{
    QString s = p;
    while (s.endsWith('/') || s.endsWith('\\')) s.chop(1);
    return s;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QString root = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::currentPath();
    const QString clsModel  = norm(root) + "/models/defect_cls.onnx";
    const QString detModel  = norm(root) + "/models/yolov8n.onnx";
    const QString sampleDir = norm(root) + "/testdata/real_samples";
    const QString outPath   = norm(root) + "/qc_ai_infer_result.txt";

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::printf("[FAIL] 无法写结果文件: %s\n", outPath.toLocal8Bit().constData());
        return 2;
    }
    QTextStream ts(&out);
    ts.setEncoding(QStringConverter::Utf8);
    auto F = [&](const QString& line) { ts << line << "\n"; };

    int fails = 0;
    F(QString("发布包AI真实推理验证  root=%1").arg(root));

    // ===== 1) 分类节点 =====
    F("===== 分类节点 (models/defect_cls.onnx) =====");
    QString err;
    auto cls = InferEngine::acquire(clsModel, &err);
    if (!cls) {
        F(QString("[FAIL] 分类模型加载: %1").arg(err));
        ++fails;
    } else if (cls->outputKind() != InferEngine::OutputKind::Classification) {
        F("[SKIP] 非分类模型");
    } else {
        const QStringList imgs = QDir(sampleDir).entryList({"*.png", "*.jpg"},
                                                           QDir::Files, QDir::Name);
        QElapsedTimer t;
        t.start();
        int n = 0;
        for (const QString& f : imgs) {
            cv::Mat img = cv::imread((sampleDir + "/" + f).toLocal8Bit().toStdString(),
                                     cv::IMREAD_COLOR);
            if (img.empty()) { F(QString("[跳过] 读取失败: %1").arg(f)); continue; }
            QList<QPair<QString, float>> res;
            if (!cls->classify(img, res, &err)) {
                F(QString("[FAIL] %1 推理: %2").arg(f, err));
                ++fails;
                continue;
            }
            QString line = QString("  %1 ->").arg(f);
            for (int i = 0; i < res.size() && i < 3; ++i)
                line += QString("  %1(%2)").arg(res[i].first).arg(res[i].second, 0, 'f', 4);
            F(line);
            ++n;
        }
        F(QString("分类完成: %1 张, 平均 %2ms/张").arg(n).arg(n ? t.elapsed() / n : 0));
    }

    // ===== 2) 检测节点 =====
    F("===== 检测节点 (models/yolov8n.onnx) =====");
    auto det = InferEngine::acquire(detModel, &err);
    if (!det) {
        F(QString("[FAIL] 检测模型加载: %1").arg(err));
        ++fails;
    } else {
        const QStringList imgs = QDir(sampleDir).entryList({"*.png", "*.jpg"},
                                                           QDir::Files, QDir::Name);
        QElapsedTimer t;
        t.start();
        int n = 0;
        for (const QString& f : imgs) {
            cv::Mat img = cv::imread((sampleDir + "/" + f).toLocal8Bit().toStdString(),
                                     cv::IMREAD_COLOR);
            if (img.empty()) continue;
            const auto dets = det->detectYolo(img, 0.25f, 0.45f, &err);
            if (!err.isEmpty()) {
                F(QString("[FAIL] %1 检测: %2").arg(f, err));
                ++fails;
                continue;
            }
            QString line = QString("  %1 -> 检出 %2 个目标").arg(f).arg((int)dets.size());
            for (size_t i = 0; i < dets.size() && i < 3; ++i)
                line += QString(" ; [cls%1 %2 %3 @%4,%5 %6x%7]")
                            .arg(dets[i].classId)
                            .arg(dets[i].className)
                            .arg(dets[i].score, 0, 'f', 2)
                            .arg(dets[i].x, 0, 'f', 1).arg(dets[i].y, 0, 'f', 1)
                            .arg(dets[i].w, 0, 'f', 1).arg(dets[i].h, 0, 'f', 1);
            F(line);
            ++n;
        }
        F(QString("检测完成: %1 张, 平均 %2ms/张").arg(n).arg(n ? t.elapsed() / n : 0));
    }

    F(fails == 0
          ? ">> 结果: 推理全部成功 (非冒烟, 真实模型加载+推理)"
          : QString(">> 结果: 存在失败 %1").arg(fails));
    out.close();

    std::printf("done fails=%d out=%s\n", fails, outPath.toLocal8Bit().constData());
    return fails == 0 ? 0 : 1;
}