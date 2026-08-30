/**
 * @file test_tools_regression.cpp
 * @brief 检测工具回归测试 — 用 testdata 合成图库验证工具链
 *
 * 测试内容:
 *   1. 快速找圆(EdgeDrawing): 测试图找到工件圆 (半径110±6); 毛刺图走 Hough 卡尺回退定位
 *   2. 检测圆形(亚像素): 用快速找圆结果作ROI中心(数据流链式), 半径110±3
 *   3. 检测直线(亚像素): 合成斜线图, 角度误差<0.5°
 *
 * 前置: 先执行 generate_testdata 目标生成 build/release/bin/testdata/virtual_camera/
 */

#include "../../src/engine/ToolRegistry.h"
#include "../../src/engine/FlowEngine.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <QCoreApplication>
#include <QDir>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;
static int g_checks = 0;
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
        std::printf("[跳过] 无测试图片, 请先构建 generate_testdata 目标\n");
        return 0;
    }
    std::printf("测试图库: %d 张\n", images.size());

    auto& reg = ToolRegistry::instance();

    int circleFinds = 0, subpixOk = 0;
    double worstRadiusErr = 0;

    for (const QString& name : images) {
        const QString path = d.absoluteFilePath(name);
        cv::Mat img = cv::imread(path.toLocal8Bit().toStdString(), cv::IMREAD_GRAYSCALE);
        if (img.empty()) continue;

        // ---- 1. 快速找圆 (EdgeDrawing) ----
        ITool* finder = reg.createTool("EdgeCircleFind");
        if (!finder) {
            std::printf("[FAIL] EdgeCircleFind 未注册\n");
            return 1;
        }
        finder->setInstanceName("快速找圆");
        finder->setProperty("minRadius", 80.0);
        finder->setProperty("maxRadius", 140.0);
        ToolContext ctx;
        ctx.setCurrentImage(std::make_shared<CvImage>(img));
        const bool found = finder->execute(ctx);
        if (found) {
            ++circleFinds;
            const double r = finder->resultData().value("radius").toDouble();
            const double cx = finder->resultData().value("centerX").toDouble();
            const double cy = finder->resultData().value("centerY").toDouble();
            CHECK(std::fabs(r - 110.0) <= 6.0,
                  QString("%1: 找圆半径%2(期望110±6)").arg(name).arg(r).toLocal8Bit().constData());
            CHECK(std::hypot(cx - img.cols / 2.0, cy - img.rows / 2.0) <= 160.0,
                  QString("%1: 找圆圆心偏离过大").arg(name).toLocal8Bit().constData());

            // ---- 2. 检测圆形(亚像素), ROI中心来自快速找圆 ----
            ITool* circleTool = reg.createTool("CircleDetection");
            circleTool->setInstanceName("检测圆形");
            circleTool->setProperty("roiCenterX", cx);
            circleTool->setProperty("roiCenterY", cy);
            circleTool->setProperty("roiRadius", 110.0);
            circleTool->setProperty("roiThickness", 24.0);
            circleTool->setProperty("scanCount", 24);
            circleTool->setProperty("gradientThreshold", 25);
            ToolContext ctx2;
            ctx2.setCurrentImage(std::make_shared<CvImage>(img));
            if (circleTool->execute(ctx2)) {
                ++subpixOk;
                const double r2 = circleTool->resultData().value("radius").toDouble();
                const double cx2 = circleTool->resultData().value("centerX").toDouble();
                const double cy2 = circleTool->resultData().value("centerY").toDouble();
                const double radErr = std::fabs(r2 - 110.0);
                worstRadiusErr = std::max(worstRadiusErr, radErr);
                CHECK(radErr <= 3.0,
                      QString("%1: 亚像素半径误差%2(<=3)").arg(name).arg(radErr, 0, 'f', 2)
                          .toLocal8Bit().constData());
                CHECK(std::hypot(cx2 - cx, cy2 - cy) <= 3.0,
                      QString("%1: 两工具圆心差>3px").arg(name).toLocal8Bit().constData());
            } else {
                CHECK(false, QString("%1: 亚像素找圆失败: %2").arg(name,
                    circleTool->resultData().value("error").toString())
                    .toLocal8Bit().constData());
            }
            delete circleTool;
        } else {
            CHECK(false, QString("%1: 快速找圆失败 err=%2 candidates=%3")
                .arg(name, finder->resultData().value("error").toString(),
                     finder->resultData().value("candidateCount").toString())
                .toLocal8Bit().constData());
        }
        delete finder;
    }

    // ---- 3. 检测直线: 合成20°斜线 ----
    {
        cv::Mat img(480, 640, CV_8UC1, cv::Scalar(45));
        cv::Point2d c(320, 240);
        const double expectAngle = 20.0 * CV_PI / 180.0;
        cv::Point2d dir(std::cos(expectAngle), std::sin(expectAngle));
        cv::line(img, c + dir * 250.0, c - dir * 250.0, cv::Scalar(230), 3, cv::LINE_AA);

        ITool* lineTool = reg.createTool("LineDetection");
        lineTool->setInstanceName("检测直线");
        lineTool->setProperty("roiCenterX", 320.0);
        lineTool->setProperty("roiCenterY", 240.0);
        lineTool->setProperty("roiWidth", 400.0);
        lineTool->setProperty("roiHeight", 60.0);
        lineTool->setProperty("roiAngle", 20.0);
        lineTool->setProperty("scanCount", 20);
        lineTool->setProperty("gradientThreshold", 30);
        ToolContext ctx;
        ctx.setCurrentImage(std::make_shared<CvImage>(img));
        CHECK(lineTool->execute(ctx), "找线执行成功");
        const double angle = lineTool->resultData().value("angle").toDouble();
        const double diff = std::fabs(angle - 20.0);
        const double angleErr = std::min(diff, 180.0 - diff);   // 直线有180°对称性
        CHECK(angleErr < 0.5,
              QString("找线角度%1° 误差%2°(<0.5)").arg(angle).arg(angleErr, 0, 'f', 3)
                  .toLocal8Bit().constData());
        delete lineTool;
    }

    // ---- 4. 位置补正数据流 (P0-1): ROI自动跟随平移+旋转 ----
    // 补正矩阵: origin=(60,40), 角度=10°  → 参考点(300,220)映射到图像坐标
    {
        const double cosA = std::cos(10.0 * CV_PI / 180.0);
        const double sinA = std::sin(10.0 * CV_PI / 180.0);
        // x_img = 60 + 300*cosA - 220*sinA, y_img = 40 + 300*sinA + 220*cosA
        const double expX = 60 + 300 * cosA - 220 * sinA;   // ≈317.24
        const double expY = 40 + 300 * sinA + 220 * cosA;   // ≈308.75
        const double imgAngle = 30.0;   // 参考ROI角20° + 补正角10°

        // 图像: 在补正后的位置/角度画一条亮线
        cv::Mat img(480, 640, CV_8UC1, cv::Scalar(45));
        cv::Point2d c(expX, expY);
        const double rad = imgAngle * CV_PI / 180.0;
        cv::Point2d dir(std::cos(rad), std::sin(rad));
        cv::line(img, c + dir * 220.0, c - dir * 220.0, cv::Scalar(230), 3, cv::LINE_AA);

        // 上下文注入补正矩阵 (模拟 位置补正/坐标系统 工具的输出)
        ToolContext ctx;
        ctx.setCurrentImage(std::make_shared<CvImage>(img));
        ctx.setData("coord_cos", cosA);
        ctx.setData("coord_sin", sinA);
        ctx.setData("coord_originX", 60.0);
        ctx.setData("coord_originY", 40.0);
        ctx.setData("coord_angle", 10.0);

        // 开启跟随: ROI 按"参考坐标"(300,220,20°)定义, 应被变换到(317,309,30°)
        ITool* posTool = reg.createTool("LineDetection");
        posTool->setInstanceName("补正跟随");
        posTool->setProperty("useCorrection", true);
        posTool->setProperty("roiCenterX", 300.0);
        posTool->setProperty("roiCenterY", 220.0);
        posTool->setProperty("roiWidth", 260.0);
        posTool->setProperty("roiHeight", 40.0);
        posTool->setProperty("roiAngle", 20.0);
        posTool->setProperty("scanCount", 20);
        posTool->setProperty("gradientThreshold", 30);
        const bool posFound = posTool->execute(ctx);
        CHECK(posFound, "补正跟随: 执行成功");
        if (posFound) {
            const double cxr = posTool->resultData().value("centerX").toDouble();
            const double cyr = posTool->resultData().value("centerY").toDouble();
            const double angR = posTool->resultData().value("angle").toDouble();
            CHECK(std::hypot(cxr - expX, cyr - expY) <= 4.0,
                  QString("补正跟随: 线中心(%.1f,%.1f) 期望(%.1f,%.1f)")
                      .arg(cxr).arg(cyr).arg(expX).arg(expY).toLocal8Bit().constData());
            double d = std::fabs(angR - imgAngle);
            d = std::min(d, 180.0 - d);
            CHECK(d < 0.8, QString("补正跟随: 角度%1° 期望%2°").arg(angR).arg(imgAngle)
                               .toLocal8Bit().constData());
        }
        delete posTool;

        // 反向对照: 不开启跟随, ROI停留在参考坐标(300,220,20°) → 应找不到补正后的线
        ITool* noTool = reg.createTool("LineDetection");
        noTool->setInstanceName("不跟随");
        noTool->setProperty("useCorrection", false);
        noTool->setProperty("roiCenterX", 300.0);
        noTool->setProperty("roiCenterY", 220.0);
        noTool->setProperty("roiWidth", 120.0);   // 窄ROI, 补正后线(317,309)落在其外
        noTool->setProperty("roiHeight", 24.0);
        noTool->setProperty("roiAngle", 20.0);
        noTool->setProperty("scanCount", 12);
        noTool->setProperty("gradientThreshold", 30);
        const bool noFound = noTool->execute(ctx);
        CHECK(!noFound, "不跟随: 参考坐标ROI应找不到补正后的线(反向对照)");
        delete noTool;
    }

    std::printf("\n回归结果: %d项检查, 硬失败%d | 找圆%d/%d | 亚像素%d/%d | 最差半径误差%.2fpx\n",
                g_checks, g_failures, circleFinds, images.size(),
                subpixOk, images.size(), worstRadiusErr);
    return g_failures == 0 ? 0 : 1;
}
