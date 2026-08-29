/**
 * @file test_thread.cpp
 * @brief 螺纹检测工具回归测试 — 用 testdata 合成螺纹图库
 *
 * 样本(每图36牙或缺口): OK_01..24 (36牙) / NG_MISSING (缺牙) / NG_DAMAGED (烂牙)
 *                       / NG_BURR (毛刺) / NG_SLANT (斜牙)
 * 验证:
 *   1. OK图: 牙数=36±1, 缺牙=0
 *   2. NG_MISSING: 缺牙检出>=1
 *   3. 全体: 牙距一致性 (36牙 -> 10°±0.8°)
 * 无头运行, 硬失败>0返回1。
 */

#include "../../src/engine/ToolRegistry.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <QCoreApplication>
#include <QDir>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0, g_checks = 0;
#define CHECK(cond, msg) do { \
    ++g_checks; \
    if (cond) { } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QString dir = QCoreApplication::applicationDirPath() + "/testdata/virtual_camera";
    QDir d(dir);
    const QStringList images = d.entryList({"*.png"}, QDir::Files, QDir::Name);
    if (images.isEmpty()) {
        std::printf("[跳过] 无测试图片\n");
        return 0;
    }

    auto& reg = ToolRegistry::instance();

    int okPassed = 0, missingCaught = 0, okTotal = 0, missTotal = 0, total = 0;
    double worstPitchErr = 0;

    for (const QString& name : images) {
        cv::Mat img = cv::imread(d.absoluteFilePath(name).toLocal8Bit().toStdString(),
                                 cv::IMREAD_GRAYSCALE);
        if (img.empty()) continue;
        ++total;

        // 链式定位: 先快速找圆取工件中心 (生产流程同款)
        ITool* finder = reg.createTool("EdgeCircleFind");
        finder->setProperty("minRadius", 80.0);
        finder->setProperty("maxRadius", 140.0);
        ToolContext ctxF;
        ctxF.setCurrentImage(std::make_shared<CvImage>(img));
        double pcx = -1, pcy = -1;
        if (finder->execute(ctxF)) {
            pcx = finder->resultData().value("centerX").toDouble();
            pcy = finder->resultData().value("centerY").toDouble();
        }
        delete finder;

        ITool* tool = reg.createTool("ThreadInspection");
        if (!tool) { std::printf("[FAIL] ThreadInspection 未注册\n"); return 1; }
        tool->setInstanceName("螺纹检测");
        tool->setProperty("roiCenterX", pcx);
        tool->setProperty("roiCenterY", pcy);
        tool->setProperty("partRadius", 110.0);
        tool->setProperty("expectedTeeth", 36);
        tool->setProperty("angularSamples", 1440);
        ToolContext ctx;
        ctx.setCurrentImage(std::make_shared<CvImage>(img));
        const bool ok = tool->execute(ctx);
        const int toothCount = tool->resultData().value("toothCount").toInt();
        const int missing = tool->resultData().value("missingCount").toInt();
        const double pitchDeg = tool->resultData().value("pitchDeg").toDouble();

        const bool isMissing = name.startsWith("NG_MISSING");
        if (isMissing) {
            ++missTotal;
            CHECK(missing >= 1, QString("%1: 缺牙未检出(missing=%2)").arg(name).arg(missing)
                                    .toLocal8Bit().constData());
            if (missing >= 1) ++missingCaught;
        } else if (name.startsWith("OK_")) {
            ++okTotal;
            CHECK(std::abs(toothCount - 36) <= 1,
                  QString("%1: 牙数%2(期望36±1) [raw=%3 maxAmp=%4 smoothW=%5 center=(%6,%7)]")
                      .arg(name).arg(toothCount)
                      .arg(tool->resultData().value("dbgRawPeaks").toInt())
                      .arg(tool->resultData().value("dbgMaxAmp").toDouble(), 0, 'f', 1)
                      .arg(tool->resultData().value("dbgSmoothW").toInt())
                      .arg(tool->resultData().value("dbgCenterX").toDouble(), 0, 'f', 1)
                      .arg(tool->resultData().value("dbgCenterY").toDouble(), 0, 'f', 1)
                      .toLocal8Bit().constData());
            CHECK(missing == 0, QString("%1: 误报缺牙%2个").arg(name).arg(missing)
                                    .toLocal8Bit().constData());
            if (std::abs(toothCount - 36) <= 1 && missing == 0) ++okPassed;
        }
        if (toothCount >= 30) {   // 牙距只对正常计数图检查
            const double perr = std::fabs(pitchDeg - 10.0);
            worstPitchErr = std::max(worstPitchErr, perr);
            CHECK(perr <= 0.8, QString("%1: 牙距%2°(期望10±0.8)").arg(name)
                                   .arg(pitchDeg, 0, 'f', 2).toLocal8Bit().constData());
        }
        delete tool;
    }

    std::printf("\n螺纹回归: %d图 | OK牙数+无缺牙 %d/%d | 缺牙检出 %d/%d | 最差牙距误差%.2f° | 硬失败%d\n",
                total, okPassed, okTotal, missingCaught, missTotal, worstPitchErr, g_failures);
    return g_failures == 0 ? 0 : 1;
}
