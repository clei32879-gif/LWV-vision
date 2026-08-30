/**
 * @file test_ai.cpp
 * @brief AI推理功能测试 — yolov8n.onnx + 真实样本图
 *
 * 验证: 模型加载 / letterbox预处理 / 推理执行 / 后处理解码 / 延迟合理
 * (COCO预训练模型对螺丝类图未必有检出 — 本测试验证管线功能与性能,
 *  语义正确性由阶段4后续用自训缺陷模型验证)
 * 前置: testdata/yolov8n.onnx 存在
 */

#include "../../src/ai/InferEngine.h"
#include <opencv2/imgcodecs.hpp>
#include <QCoreApplication>
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
    QString model = appDir + "/models/yolov8n.onnx";
    if (!QFileInfo::exists(model))
        model = appDir + "/testdata/yolov8n.onnx";
    if (!QFileInfo::exists(model)) {
        std::printf("[跳过] 未找到 yolov8n.onnx\n");
        return 0;
    }

    // 1. 模型加载
    std::printf("测试1: 模型加载\n");
    QString err;
    auto engine = InferEngine::acquire(model, &err);
    CHECK(engine != nullptr, QString("模型加载: %1").arg(err).toLocal8Bit().constData());
    if (!engine) return 1;
    CHECK(engine->inputWidth() == 640 && engine->inputHeight() == 640,
          QString("输入尺寸 %1x%2 (=640)").arg(engine->inputWidth())
              .arg(engine->inputHeight()).toLocal8Bit().constData());
    std::printf("  执行提供器: %s\n",
                engine->executionProvider().toLocal8Bit().constData());

    // 2. 真实样本图推理
    std::printf("测试2: 真实样本图推理(螺丝长_良品)\n");
    QString img = appDir + "/testdata/real_samples/螺丝长_良品_01.png";
    if (!QFileInfo::exists(img))
        img = appDir + "/../../../testdata/real_samples/螺丝长_良品_01.png";  // 源目录回退
    cv::Mat bgr = cv::imread(img.toLocal8Bit().toStdString(), cv::IMREAD_COLOR);
    if (bgr.empty()) {
        std::printf("  [跳过] 样本图缺失: %s\n", img.toLocal8Bit().constData());
    } else {
        QElapsedTimer timer;
        timer.start();
        const auto dets = engine->detectYolo(bgr, 0.25f, 0.45f, &err);
        const qint64 ms = timer.elapsed();
        CHECK(err.isEmpty(), QString("推理执行: %1").arg(err).toLocal8Bit().constData());
        CHECK(ms < 5000, QString("推理耗时 %1ms (<5000, CPU)").arg(ms).toLocal8Bit().constData());
        bool inBounds = true;
        for (const auto& d : dets) {
            if (d.x < -50 || d.y < -50 || d.x + d.w > bgr.cols + 50 || d.y + d.h > bgr.rows + 50)
                inBounds = false;
        }
        CHECK(inBounds, QString("检出%1个框且坐标在合理范围").arg((int)dets.size())
                            .toLocal8Bit().constData());
        std::printf("  (检出 %d 个目标, 耗时 %lldms)\n", (int)dets.size(), ms);
    }

    // 3. 模型缓存复用
    std::printf("测试3: 模型缓存复用\n");
    auto engine2 = InferEngine::acquire(model, &err);
    CHECK(engine2 == engine, "相同路径共享同一Session");

    std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
