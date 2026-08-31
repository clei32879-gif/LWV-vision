/**
 * @file test_ocr.cpp
 * @brief OCR 真实功能验证 — 合成字符图: 教导→识别→断言文本
 *
 * 用 OpenCV putText 生成 "LW-VISION 2026" 印刷体图,
 * 1) 教导模式: 喂图+期望文本 → 模板写入 models/ocr_templates.ini
 * 2) 识别模式: 喂同样图 → 输出文本应等于期望文本
 * 3) 泛化: 喂稍不同字号的图 → 识别率 >= 80%
 */

#include "../../src/engine/ToolRegistry.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [OK] %s\n", msg); } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

static cv::Mat makeTextImage(const std::string& text, double scale = 1.0,
                             cv::Scalar bg = cv::Scalar(40)) {
    cv::Mat img(80, 560, CV_8UC1, bg);
    cv::putText(img, text, cv::Point(10, 55), cv::FONT_HERSHEY_SIMPLEX,
                0.9 * scale, cv::Scalar(230), 2, cv::LINE_AA);
    return img;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    auto& reg = ToolRegistry::instance();

    const QString teachImg = "LW-VISION 2026";
    const QString otherImg = "LW-VISION 2027";   // 只差最后一位 — 泛化测试

    // ---- 1. 教导 ----
    std::printf("步骤1: 教导 (文本: %s)\n", teachImg.toLocal8Bit().constData());
    ITool* ocr = reg.createTool("OCR");
    if (!ocr) { std::printf("[FAIL] OCR未注册\n"); return 1; }
    ocr->setInstanceName("字符识别");
    ocr->setProperty("mode", 1);          // 教导
    ocr->setProperty("teachText", teachImg);
    ocr->setProperty("useROI", true);
    ocr->setProperty("roiX", 0); ocr->setProperty("roiY", 0);
    ocr->setProperty("roiW", 560); ocr->setProperty("roiH", 80);
    ocr->setProperty("thresholdValue", 0);   // Otsu
    ocr->setProperty("invert", true);   // 白字黑底 → THRESH_BINARY
    ocr->setProperty("minCharHeight", 12);   // 工具默认; putText 0.9字高约18px
    ocr->setProperty("maxCharHeight", 100);
    ocr->setProperty("charGap", 25);

    cv::Mat teach = makeTextImage(teachImg.toStdString());
    ToolContext ctx1;
    ctx1.setCurrentImage(std::make_shared<CvImage>(teach));
    const bool taught = ocr->execute(ctx1);
    CHECK(taught, QString("教导执行: %1").arg(
        ocr->resultData().value("error").toString()).toLocal8Bit().constData());
    if (taught) {
        CHECK(ocr->resultData().value("taughtCount").toInt() == 13,
              QString("教导%1个字符").arg(
                  ocr->resultData().value("taughtCount").toInt()).toLocal8Bit().constData());
    }

    // ---- 2. 识别同一图 ----
    std::printf("步骤2: 识别同一图\n");
    ocr->setProperty("mode", 0);          // 识别
    cv::Mat same = makeTextImage(teachImg.toStdString());
    ToolContext ctx2;
    ctx2.setCurrentImage(std::make_shared<CvImage>(same));
    const bool ok2 = ocr->execute(ctx2);
    CHECK(ok2, "识别执行成功");
    if (ok2) {
        QString text = ocr->resultData().value("recognizedText").toString();
        std::printf("  识别结果: [%s]\n", text.toLocal8Bit().constData());
        text.remove(' ');
        CHECK(text == QString(teachImg).remove(' '), "同一图识别文本一致");
    }

    // ---- 3. 泛化: 不同字号的同文本 ----
    std::printf("步骤3: 泛化 (字号+20%%)\n");
    cv::Mat bigger = makeTextImage(teachImg.toStdString(), 1.2);
    ToolContext ctx3;
    ctx3.setCurrentImage(std::make_shared<CvImage>(bigger));
    const bool ok3 = ocr->execute(ctx3);
    CHECK(ok3, "泛化识别执行成功");
    if (ok3) {
        const QString text = ocr->resultData().value("recognizedText").toString();
        std::printf("  识别结果: [%s]\n", text.toLocal8Bit().constData());
        // 对齐比较(跳过'-', 字号变化时横杠可能并入相邻字符)
        QString expect = teachImg, got = text;
        expect.remove('-'); expect.remove(' ');
        got.remove('-'); got.remove(' ');
        int match = 0, total = qMin(expect.length(), got.length());
        for (int i = 0; i < total; ++i)
            if (expect[i] == got[i]) ++match;
        CHECK(total > 0 && match * 100 / total >= 80,
              QString("泛化匹配 %1/%2 (>=80%%)").arg(match).arg(total).toLocal8Bit().constData());
    }

    // ---- 4. 变体文本 (最后一位不同) ----
    std::printf("步骤4: 变体文本 (%s)\n", otherImg.toLocal8Bit().constData());
    cv::Mat diff = makeTextImage(otherImg.toStdString());
    ToolContext ctx4;
    ctx4.setCurrentImage(std::make_shared<CvImage>(diff));
    const bool ok4 = ocr->execute(ctx4);
    if (ok4) {
        const QString text = ocr->resultData().value("recognizedText").toString();
        std::printf("  识别结果: [%s]\n", text.toLocal8Bit().constData());
        // '7'未教导过 → 合理输出为 '?' 或形近字符 — 前缀(去空格)LW-VISION202识别正确即可
        CHECK(QString(text).remove(' ').startsWith("LW-VISION202"),
              "变体文本前缀识别正确(未教导字符输出?可接受)");
    }

    // 清理测试模板 (不污染正式模型)
    QFile::remove(QCoreApplication::applicationDirPath() + "/models/ocr_templates.ini");

    std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "OCR全流程通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
