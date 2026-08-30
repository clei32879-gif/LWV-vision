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
#include "../../src/core/GlobalVariables.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#include <opencv2/geometry/3d.hpp>
#include <opencv2/calib.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <atomic>
#include <cstdio>
#include <cmath>
#include <algorithm>

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

    // ---- 5. 几何测量工具 (对齐CKVision"几何对象→几何运算"链) ----
    {
        // 两线交点: 线1水平(320,240,0°), 线2垂直(400,240,90°) → 交于(400,240)
        ITool* li = reg.createTool("LineIntersect");
        li->setInstanceName("两线交点");
        li->setProperty("line1CenterX", "320"); li->setProperty("line1CenterY", "240");
        li->setProperty("line1Angle", "0");
        li->setProperty("line2CenterX", "400"); li->setProperty("line2CenterY", "240");
        li->setProperty("line2Angle", "90");
        ToolContext ctx;
        CHECK(li->execute(ctx), "两线交点执行成功");
        const double ix = li->resultData().value("intersectX").toDouble();
        const double iy = li->resultData().value("intersectY").toDouble();
        const double ib = li->resultData().value("angleBetween").toDouble();
        CHECK(std::hypot(ix - 400.0, iy - 240.0) <= 1e-6,
              QString("两线交点(%.1f,%.1f) 期望(400,240)").arg(ix).arg(iy)
                  .toLocal8Bit().constData());
        CHECK(std::fabs(ib - 90.0) <= 1e-6,
              QString("两线夹角%1° 期望90°").arg(ib).toLocal8Bit().constData());
        // 平行对照
        ITool* li2 = reg.createTool("LineIntersect");
        li2->setInstanceName("两线交点-平行");
        li2->setProperty("line1CenterX", "0"); li2->setProperty("line1CenterY", "0");
        li2->setProperty("line1Angle", "45");
        li2->setProperty("line2CenterX", "0"); li2->setProperty("line2CenterY", "50");
        li2->setProperty("line2Angle", "45");
        ToolContext ctx2;
        li2->execute(ctx2);
        CHECK(li2->resultData().value("parallel").toBool(), "平行线标记parallel=true");
        delete li; delete li2;

        // 点到线距离: 点(320,200), 水平线过(320,240,0°) → 距离40, 垂足(320,240)
        ITool* ptl = reg.createTool("PointToLine");
        ptl->setInstanceName("点到线");
        ptl->setProperty("pointX", "320"); ptl->setProperty("pointY", "200");
        ptl->setProperty("lineCenterX", "320"); ptl->setProperty("lineCenterY", "240");
        ptl->setProperty("lineAngle", "0");
        ToolContext ctx3;
        CHECK(ptl->execute(ctx3), "点到线执行成功");
        const double dist = ptl->resultData().value("distance").toDouble();
        const double fxx = ptl->resultData().value("footX").toDouble();
        const double fyy = ptl->resultData().value("footY").toDouble();
        CHECK(std::fabs(dist - 40.0) <= 1e-6,
              QString("点到线距离%1 期望40").arg(dist, 0, 'f', 3).toLocal8Bit().constData());
        CHECK(std::hypot(fxx - 320.0, fyy - 240.0) <= 1e-6,
              QString("垂足(%.1f,%.1f) 期望(320,240)").arg(fxx).arg(fyy)
                  .toLocal8Bit().constData());
        delete ptl;
    }

    // ---- 6. EdgeDetection "全部"边缘输出 + 位置选择 (M-42) ----
    {
        // 合成阶跃图: 左黑右白, 边界在 x=320 (竖直亮-暗边缘)
        cv::Mat img(100, 640, CV_8UC1, cv::Scalar(20));
        cv::rectangle(img, cv::Rect(320, 0, 320, 100), cv::Scalar(220), cv::FILLED);

        ToolContext ctx;
        ctx.setCurrentImage(std::make_shared<CvImage>(img));

        // "全部"模式: 应输出 edgePoints 列表且不为空
        ITool* edAll = reg.createTool("EdgeDetection");
        edAll->setInstanceName("边缘-全部");
        edAll->setProperty("roiType", 1);           // 矩形
        edAll->setProperty("roiCenterX", 320.0);
        edAll->setProperty("roiCenterY", 50.0);
        edAll->setProperty("roiWidth", 300.0);
        edAll->setProperty("roiHeight", 40.0);
        edAll->setProperty("edgePosition", 4);      // 全部
        edAll->setProperty("gradientThreshold", 30);
        edAll->setProperty("filterHalfWidth", 2);   // M-42: 平滑参数应生效
        CHECK(edAll->execute(ctx), "边缘-全部: 执行成功");
        const auto all = edAll->resultData().value("edgePoints").toList();
        CHECK(!all.isEmpty(), "边缘-全部: edgePoints 列表非空");
        if (!all.isEmpty()) {
            // edgePoints 为嵌套 {x,y,grad} 三元组
            const QVariantList first = all.first().toList();
            CHECK(first.size() == 3, "边缘-全部: 三元组含3元素");
            const double ex = first.value(0).toDouble();
            CHECK(std::fabs(ex - 320.0) <= 3.0,
                  QString("边缘-全部: 首边缘x=%.1f 期望≈320").arg(ex).toLocal8Bit().constData());
        }
        delete edAll;

        // "最强"模式: 应选最强梯度边缘 (黑→白在x=320附近)
        ITool* edMax = reg.createTool("EdgeDetection");
        edMax->setInstanceName("边缘-最强");
        edMax->setProperty("roiType", 1);
        edMax->setProperty("roiCenterX", 320.0);
        edMax->setProperty("roiCenterY", 50.0);
        edMax->setProperty("roiWidth", 300.0);
        edMax->setProperty("roiHeight", 40.0);
        edMax->setProperty("edgePosition", 3);      // 最强
        edMax->setProperty("gradientThreshold", 30);
        CHECK(edMax->execute(ctx), "边缘-最强: 执行成功");
        const double mx = edMax->resultData().value("positionX").toDouble();
        CHECK(std::fabs(mx - 320.0) <= 3.0,
              QString("边缘-最强: x=%.1f 期望≈320").arg(mx).toLocal8Bit().constData());
        delete edMax;
    }

    // ---- 7. 标定真实化: 棋盘格角点检测 + 自动比例 (P0-3) ----
    // testdata_gen 生成的棋盘格: 内角点9x6, 单格60px, 实际10mm → 理论比例 10/60≈0.1667
    {
        const QString calDir = d.absoluteFilePath("calib_board");
        QDir cd(calDir);
        const QStringList boards = cd.entryList({"*.png"}, QDir::Files, QDir::Name);
        for (const QString& name : boards) {
            const QString path = cd.absoluteFilePath(name);
            cv::Mat img = cv::imread(path.toLocal8Bit().toStdString(), cv::IMREAD_GRAYSCALE);
            if (img.empty()) continue;

            ToolContext ctx;
            ctx.setCurrentImage(std::make_shared<CvImage>(img));
            ITool* cal = reg.createTool("Calibration");
            cal->setInstanceName("棋盘格标定");
            cal->setProperty("calibMethod", 2);      // 棋盘格标定
            cal->setProperty("boardCols", 9);
            cal->setProperty("boardRows", 6);
            cal->setProperty("squareSize", 10.0);    // mm
            const bool ok = cal->execute(ctx);
            CHECK(ok, QString("棋盘格标定[%1]: 检测成功").arg(name).toLocal8Bit().constData());
            if (ok) {
                const double ratio = cal->resultData().value("mmPerPixel").toDouble();
                const int cnt = cal->resultData().value("cornerCount").toInt();
                CHECK(cnt == 54, QString("棋盘格标定[%1]: 角点数%2 期望54")
                            .arg(name).arg(cnt).toLocal8Bit().constData());
                // 理论比例 10mm/60px = 0.1667, 容差±5%
                CHECK(std::fabs(ratio - 10.0 / 60.0) < 0.01,
                      QString("棋盘格标定[%1]: 比例%2 期望≈0.1667")
                          .arg(name).arg(ratio, 0, 'f', 4).toLocal8Bit().constData());
                // 上下文写入校验
                const double ctxRatio = ctx.getDouble("calibration_ratio", 0);
                CHECK(std::fabs(ctxRatio - ratio) < 1e-9, "棋盘格标定: 上下文比例一致");
            }
            delete cal;
        }
        CHECK(boards.size() >= 2, "棋盘格测试资产≥2张(正交+旋转)");
    }

    // ---- 8. 几何组合测量: 圆到线距离 / 圆到圆距离 (CKVision"线到圆/圆到圆") ----
    {
        // 圆到线: 圆心(320,200), 水平线过(320,240,0°) → 距离40, 垂足(320,200)
        ITool* ctl = reg.createTool("CircleToLine");
        ctl->setInstanceName("圆到线");
        ctl->setProperty("circleCenterX", "320"); ctl->setProperty("circleCenterY", "200");
        ctl->setProperty("lineCenterX", "320");   ctl->setProperty("lineCenterY", "240");
        ctl->setProperty("lineAngle", "0");
        ToolContext ctxCL;
        CHECK(ctl->execute(ctxCL), "圆到线: 执行成功");
        const double cld = ctl->resultData().value("distance").toDouble();
        const double clfx = ctl->resultData().value("footX").toDouble();
        const double clfy = ctl->resultData().value("footY").toDouble();
        CHECK(std::fabs(cld - 40.0) <= 1e-6,
              QString("圆到线距离%1 期望40").arg(cld, 0, 'f', 3).toLocal8Bit().constData());
        CHECK(std::hypot(clfx - 320.0, clfy - 240.0) <= 1e-6,
              QString("圆到线垂足(%.1f,%.1f) 期望(320,240)").arg(clfx).arg(clfy)
                  .toLocal8Bit().constData());
        delete ctl;

        // 圆到圆: 圆1(100,100,r=50), 圆2(160,100,r=30) → 圆心距60, 间隙-20(相交)
        ITool* ctc = reg.createTool("CircleToCircle");
        ctc->setInstanceName("圆到圆");
        ctc->setProperty("circle1CenterX", "100"); ctc->setProperty("circle1CenterY", "100");
        ctc->setProperty("circle1Radius", "50");
        ctc->setProperty("circle2CenterX", "160"); ctc->setProperty("circle2CenterY", "100");
        ctc->setProperty("circle2Radius", "30");
        ToolContext ctxCC;
        CHECK(ctc->execute(ctxCC), "圆到圆: 执行成功");
        const double cd = ctc->resultData().value("centerDistance").toDouble();
        const double gap = ctc->resultData().value("gap").toDouble();
        const QString rel = ctc->resultData().value("relation").toString();
        CHECK(std::fabs(cd - 60.0) <= 1e-6,
              QString("圆到圆圆心距%1 期望60").arg(cd, 0, 'f', 3).toLocal8Bit().constData());
        CHECK(std::fabs(gap - (-20.0)) <= 1e-6,
              QString("圆到圆间隙%1 期望-20").arg(gap, 0, 'f', 3).toLocal8Bit().constData());
        CHECK(rel == QStringLiteral("相交"), QString("圆到圆关系%1 期望相交").arg(rel)
                                                 .toLocal8Bit().constData());
        delete ctc;

        // 圆到圆 同心度: 圆心距即同心度
        ITool* ctc2 = reg.createTool("CircleToCircle");
        ctc2->setInstanceName("圆到圆-同心");
        ctc2->setProperty("circle1CenterX", "320"); ctc2->setProperty("circle1CenterY", "240");
        ctc2->setProperty("circle1Radius", "100");
        ctc2->setProperty("circle2CenterX", "320"); ctc2->setProperty("circle2CenterY", "240");
        ctc2->setProperty("circle2Radius", "50");
        ToolContext ctxCC2;
        ctc2->execute(ctxCC2);
        const double conc = ctc2->resultData().value("concentricity").toDouble();
        CHECK(std::fabs(conc) <= 1e-9, "圆到圆同心度=0(同圆心)");
        delete ctc2;
    }

    // ---- 9. 死属性激活: 旋转/缩放搜索 + Blob特征 + 顶点位置策略 (M-43) ----
    {
        // 9a. BlobAnalysis useEllipse/useBBox: 合成椭圆斑
        {
            cv::Mat img(120, 160, CV_8UC1, cv::Scalar(0));
            cv::ellipse(img, cv::Point(80, 60), cv::Size(40, 20), 30.0, 0, 360,
                        cv::Scalar(255), cv::FILLED);
            ToolContext ctx;
            ctx.setCurrentImage(std::make_shared<CvImage>(img));
            ITool* blob = reg.createTool("BlobAnalysis");
            blob->setInstanceName("斑点-特征");
            blob->setProperty("roiType", 0);
            blob->setProperty("useEllipse", true);
            blob->setProperty("useBBox", true);
            blob->setProperty("detectionType", 1);   // 白色目标(合成图为黑底白椭圆)
            blob->setProperty("threshold", 127);
            blob->setProperty("autoThreshold", false);
            blob->setProperty("minArea", 100);
            CHECK(blob->execute(ctx), "斑点-特征: 执行成功");
            const double bbw = blob->resultData().value("bboxWidth").toDouble();
            const double bbAngle = blob->resultData().value("bboxAngle").toDouble();
            const double maj = blob->resultData().value("ellipseMajor").toDouble();
            CHECK(std::fabs(bbw - 80.0) < 6.0,
                  QString("斑点-特征: 外接框长轴%1 期望≈80").arg(bbw).toLocal8Bit().constData());
            CHECK(std::fabs(bbAngle - 30.0) < 8.0,
                  QString("斑点-特征: 外接框角度%1 期望≈30").arg(bbAngle).toLocal8Bit().constData());
            CHECK(std::fabs(maj - 80.0) < 6.0,
                  QString("斑点-特征: 椭圆长轴%1 期望≈80").arg(maj).toLocal8Bit().constData());
            delete blob;
        }

        // 9b. VertexDetection detectPosition 策略: 两角点, 选离ROI中心最远(2)
        {
            cv::Mat img(100, 160, CV_8UC1, cv::Scalar(0));
            cv::rectangle(img, cv::Rect(0, 0, 160, 100), cv::Scalar(255), cv::FILLED);
            // 两个黑角点(均在扫描区 rw/4..3rw/4 内): 左上(50,30) 右下(100,55)
            cv::rectangle(img, cv::Rect(50, 30, 20, 20), cv::Scalar(0), cv::FILLED);
            cv::rectangle(img, cv::Rect(100, 55, 20, 20), cv::Scalar(0), cv::FILLED);
            ToolContext ctx;
            ctx.setCurrentImage(std::make_shared<CvImage>(img));
            ITool* vd = reg.createTool("VertexDetection");
            vd->setInstanceName("顶点-最远");
            vd->setProperty("roiType", 1);
            vd->setProperty("roiCenterX", 80.0);
            vd->setProperty("roiCenterY", 50.0);
            vd->setProperty("roiWidth", 160.0);
            vd->setProperty("roiHeight", 100.0);
            vd->setProperty("detectPosition", 2);    // 最远(离ROI中心最远的角点)
            vd->setProperty("gradientThreshold", 40);
            CHECK(vd->execute(ctx), "顶点-最远: 执行成功");
            const double px = vd->resultData().value("positionX").toDouble();
            const double py = vd->resultData().value("positionY").toDouble();
            // 右下角点(120,75)离ROI中心(80,50)最远: 期望≈(120,75)
            CHECK(std::hypot(px - 120.0, py - 75.0) < 12.0,
                  QString("顶点-最远: (%1,%2) 期望≈(120,75)")
                      .arg(px, 0, 'f', 1).arg(py, 0, 'f', 1).toLocal8Bit().constData());
            delete vd;
        }

        // 9c. ShapeMatch 旋转搜索: L形模板(非对称, 旋转可分辨), 目标含旋转30°的L形
        {
            // 模板: 白底黑色L形 (厚8, 外廓约40x40) — BINARY_INV后L变白shape被取为轮廓
            cv::Mat tmpl(80, 80, CV_8UC1, cv::Scalar(255));
            cv::rectangle(tmpl, cv::Rect(15, 15, 40, 20), cv::Scalar(0), cv::FILLED);  // 横臂
            cv::rectangle(tmpl, cv::Rect(15, 15, 20, 40), cv::Scalar(0), cv::FILLED);  // 竖臂
            const QString tmplPath = d.absoluteFilePath("_tmpl_L.png");
            cv::imwrite(tmplPath.toLocal8Bit().toStdString(), tmpl);

            // 目标: 旋转30°后的L形 (白L形, 轮廓图)
            cv::Mat tgt(200, 200, CV_8UC1, cv::Scalar(0));
            cv::rectangle(tgt, cv::Rect(80, 80, 40, 20), cv::Scalar(255), cv::FILLED);
            cv::rectangle(tgt, cv::Rect(80, 80, 20, 40), cv::Scalar(255), cv::FILLED);
            cv::Mat rot = cv::getRotationMatrix2D(cv::Point2f(100, 100), 30.0, 1.0);
            cv::Mat tgtRot;
            cv::warpAffine(tgt, tgtRot, rot, cv::Size(200, 200), cv::INTER_LINEAR,
                           cv::BORDER_CONSTANT, cv::Scalar(0));

            ToolContext ctx;
            ctx.setCurrentImage(std::make_shared<CvImage>(tgtRot));
            ITool* sm = reg.createTool("ShapeMatch");
            sm->setInstanceName("形状-旋转");
            sm->setProperty("templatePath", tmplPath);
            sm->setProperty("threshold", 0.5);
            sm->setProperty("angleRange", 40.0);     // 旋转搜索范围覆盖30°
            sm->setProperty("scaleRange", 0.3);
            const bool ok = sm->execute(ctx);
            CHECK(ok, "形状-旋转: 找到匹配");
            if (ok) {
                const double ang = sm->resultData().value("matchAngle").toDouble();
                // 旋转量幅值应≈30° (方向符号随坐标系约定, 这里只验旋转量本身)
                CHECK(std::fabs(std::fabs(ang) - 30.0) < 12.0,
                      QString("形状-旋转: 最佳角度%1 期望≈±30").arg(ang).toLocal8Bit().constData());
            }
            delete sm;
            QFile::remove(tmplPath);
        }

        // 9d. Calibration applyTo=所有流程 → 写入全局变量
        {
            ToolContext ctx;
            auto* gv = new GlobalVariables;
            ctx.setGlobalVariables(gv);
            ITool* cal = reg.createTool("Calibration");
            cal->setInstanceName("标定-全局");
            cal->setProperty("calibMethod", 1);       // 已知比例
            cal->setProperty("pixelRatio", 0.125);    // mm/px
            cal->setProperty("applyTo", 1);           // 所有流程
            CHECK(cal->execute(ctx), "标定-全局: 执行成功");
            CHECK(gv->has("calibration_ratio") && std::fabs(gv->getDouble("calibration_ratio") - 0.125) < 1e-9,
                  "标定-全局: 全局变量已写入比例");
            CHECK(std::fabs(gv->getDouble("calibration_pixel_per_mm") - 8.0) < 1e-6,
                  "标定-全局: 全局像素/毫米=8");
            delete cal;
            delete gv;
        }

        // 9e. MultiContourMatch sizeTolerance: 面积一致性过滤
        {
            cv::Mat img(120, 200, CV_8UC1, cv::Scalar(0));
            // 三个相同白斑(约30x30=900) + 一个大斑(60x60=3600)
            cv::rectangle(img, cv::Rect(20, 20, 30, 30), cv::Scalar(255), cv::FILLED);
            cv::rectangle(img, cv::Rect(90, 20, 30, 30), cv::Scalar(255), cv::FILLED);
            cv::rectangle(img, cv::Rect(160, 20, 30, 30), cv::Scalar(255), cv::FILLED);
            cv::rectangle(img, cv::Rect(70, 60, 60, 60), cv::Scalar(255), cv::FILLED);
            ToolContext ctx;
            ctx.setCurrentImage(std::make_shared<CvImage>(img));
            ITool* mcm = reg.createTool("MultiContourMatch");
            mcm->setInstanceName("多轮廓-容差");
            mcm->setProperty("roiType", 0);
            mcm->setProperty("minArea", 100);
            mcm->setProperty("maxArea", 100000);
            mcm->setProperty("sizeTolerance", 10.0);   // 只保留与中位数面积偏差≤10%的斑
            mcm->setProperty("minCount", 3);
            CHECK(mcm->execute(ctx), "多轮廓-容差: 执行成功");
            const int mc = mcm->resultData().value("matchCount").toInt();
            CHECK(mc == 3, QString("多轮廓-容差: 匹配数%1 期望3(过滤掉大斑)").arg(mc).toLocal8Bit().constData());
            const int filt = mcm->resultData().value("sizeFiltered").toInt();
            CHECK(filt == 1, QString("多轮廓-容差: 被过滤%1 期望1").arg(filt).toLocal8Bit().constData());
            delete mcm;
        }

        // 9f. CircleDetection tolerance: 拟合超差判定 (双半径边缘 → 拟合RMS大)
        {
            cv::Mat img(160, 160, CV_8UC1, cv::Scalar(0));
            cv::circle(img, cv::Point(80, 80), 50, cv::Scalar(255), 3);  // 主圆
            cv::circle(img, cv::Point(80, 80), 30, cv::Scalar(255), 3);  // 内圆(干扰, 双半径)
            ToolContext ctx;
            ctx.setCurrentImage(std::make_shared<CvImage>(img));
            ITool* cd = reg.createTool("CircleDetection");
            cd->setInstanceName("圆-容差严");
            cd->setProperty("roiCenterX", 80.0);
            cd->setProperty("roiCenterY", 80.0);
            cd->setProperty("roiRadius", 45.0);
            cd->setProperty("roiThickness", 40.0);
            cd->setProperty("scanCount", 60);
            cd->setProperty("tolerance", 0.5);          // 严容差 → 双半径必然超差 NG
            const bool cdOk = cd->execute(ctx);
            CHECK(!cdOk, "圆-容差严: 超差应NG");
            CHECK(cd->resultData().value("fitFailed").toBool(), "圆-容差严: 标记fitFailed");
            delete cd;
        }
    }

    // ---- 10. 相机标定: calibrateCamera 多图内参/畸变 (P0-3 §3.3) ----
    // testdata_gen 以已知内参 (fx=fy=800, cx=320, cy=240) + 6组姿态投影生成 calib_cam/*.png
    {
        const QString camDir = d.absoluteFilePath("calib_cam");
        QDir cd2(camDir);
        const QStringList camBoards = cd2.entryList({"*.png"}, QDir::Files, QDir::Name);
        if (camBoards.size() >= 4) {
            ToolContext ctx;
            ITool* cal = reg.createTool("Calibration");
            cal->setInstanceName("相机标定");
            cal->setProperty("calibMethod", 3);       // 相机标定(多图)
            cal->setProperty("imageDir", camDir);
            cal->setProperty("boardCols", 9);
            cal->setProperty("boardRows", 6);
            cal->setProperty("squareSize", 10.0);     // mm
            const bool ok = cal->execute(ctx);
            CHECK(ok, "相机标定: 执行成功");
            if (ok) {
                const int views = cal->resultData().value("viewCount").toInt();
                CHECK(views >= 4, QString("相机标定: 有效视角%1≥4").arg(views).toLocal8Bit().constData());
                const double fx = cal->resultData().value("fx").toDouble();
                const double fy = cal->resultData().value("fy").toDouble();
                const double cx = cal->resultData().value("cx").toDouble();
                const double cy = cal->resultData().value("cy").toDouble();
                // 真值 fx=fy=800, cx=320, cy=240; 合成图应恢复接近真值(容差10%)
                CHECK(std::fabs(fx - 800.0) < 80.0,
                      QString("相机标定: fx=%.1f 期望≈800").arg(fx).toLocal8Bit().constData());
                CHECK(std::fabs(fy - 800.0) < 80.0,
                      QString("相机标定: fy=%.1f 期望≈800").arg(fy).toLocal8Bit().constData());
                CHECK(std::fabs(cx - 320.0) < 30.0,
                      QString("相机标定: cx=%.1f 期望≈320").arg(cx).toLocal8Bit().constData());
                CHECK(std::fabs(cy - 240.0) < 30.0,
                      QString("相机标定: cy=%.1f 期望≈240").arg(cy).toLocal8Bit().constData());
                const double rms = cal->resultData().value("reprojectionError").toDouble();
                // 合成棋盘格是理想投影, 重投影误差应很小
                CHECK(rms < 0.5, QString("相机标定: 重投影误差%.3fpx<0.5").arg(rms, 0, 'f', 3).toLocal8Bit().constData());
                // 上下文写入校验
                const double ctxFx = ctx.getDouble("calibration_camera_fx", 0);
                CHECK(std::fabs(ctxFx - fx) < 1e-6, "相机标定: 上下文fx一致");
            }
            delete cal;
        } else {
            CHECK(false, QString("相机标定资产缺失: 期望≥4张实际%1").arg(camBoards.size()).toLocal8Bit().constData());
        }
    }

    // ---- 11. 坐标校准: 多点仿射/透视坐标标定 (对标CKVision坐标校准1/2) ----
    // 生成已知变换 → 反标定恢复矩阵 + 残差 + 查询点变换验证
    {
        // 已知仿射: 旋转30° + 缩放1.2 + 平移(50,-20)
        const double ang = 30.0 * M_PI / 180.0;
        const double s = 1.2, tx = 50.0, ty = -20.0;
        const double a = s * std::cos(ang), b = -s * std::sin(ang);
        const double d = s * std::sin(ang), e = s * std::cos(ang);
        // 生成 6 个源点及其目标点
        const double srcArr[6][2] = {{100, 100}, {200, 150}, {150, 250},
                                     {300, 200}, {250, 300}, {120, 80}};
        QString srcText, dstText;
        for (int i = 0; i < 6; ++i) {
            const double sx = srcArr[i][0], sy = srcArr[i][1];
            const double ddx = a * sx + b * sy + tx;
            const double ddy = d * sx + e * sy + ty;
            if (i) { srcText += ";"; dstText += ";"; }
            srcText += QString("%1,%2").arg(sx).arg(sy);
            dstText += QString("%1,%2").arg(ddx).arg(ddy);
        }
        ToolContext ctx;
        ITool* cc = reg.createTool("CoordinateCalibration");
        cc->setInstanceName("坐标校准-仿射");
        cc->setProperty("model", 0);          // 仿射
        cc->setProperty("srcPoints", srcText);
        cc->setProperty("dstPoints", dstText);
        cc->setProperty("queryX", 0.0);
        cc->setProperty("queryY", 0.0);
        CHECK(cc->execute(ctx), "坐标校准-仿射: 执行成功");
        const double rms = cc->resultData().value("rms").toDouble();
        const double rx = cc->resultData().value("resultX").toDouble();
        const double ry = cc->resultData().value("resultY").toDouble();
        // 精确拟合(6点)应零残差
        CHECK(rms < 0.01, QString("坐标校准-仿射: rms=%.4f<0.01").arg(rms).toLocal8Bit().constData());
        // 查询点(0,0)变换 = 平移分量 (50,-20)
        CHECK(std::fabs(rx - tx) < 0.01, QString("坐标校准-仿射: 变换X=%.2f 期望%.1f").arg(rx).arg(tx).toLocal8Bit().constData());
        CHECK(std::fabs(ry - ty) < 0.01, QString("坐标校准-仿射: 变换Y=%.2f 期望%.1f").arg(ry).arg(ty).toLocal8Bit().constData());
        // 上下文写入校验
        const double ctxA = ctx.getDouble("calibration_transform_0", 0);
        CHECK(std::fabs(ctxA - a) < 0.01, "坐标校准-仿射: 上下文矩阵a一致");
        // 矩阵[0]应为 a = s·cos30°
        const QVariantList ml = cc->resultData().value("matrix").toList();
        CHECK(ml.size() == 6, "坐标校准-仿射: 矩阵2x3");
        if (ml.size() == 6)
            CHECK(std::fabs(ml[0].toDouble() - a) < 0.01,
                  "坐标校准-仿射: 矩阵a分量正确");
        delete cc;

        // 透视: 已知单应 H (平移+缩放, 非纯仿射用4点)
        {
            const double h00 = 1.1, h01 = 0.2, h02 = 30.0;
            const double h10 = -0.1, h11 = 1.05, h12 = 15.0;
            const double h20 = 0.0003, h21 = 0.0001, h22 = 1.0;
            QString srcText2, dstText2;
            const double src2[5][2] = {{100, 100}, {220, 140}, {160, 260},
                                       {300, 210}, {80, 90}};
            for (int i = 0; i < 5; ++i) {
                const double sx = src2[i][0], sy = src2[i][1];
                const double w = h20 * sx + h21 * sy + h22;
                const double dx = (h00 * sx + h01 * sy + h02) / w;
                const double dy = (h10 * sx + h11 * sy + h12) / w;
                if (i) { srcText2 += ";"; dstText2 += ";"; }
                srcText2 += QString("%1,%2").arg(sx).arg(sy);
                dstText2 += QString("%1,%2").arg(dx, 0, 'f', 6).arg(dy, 0, 'f', 6);
            }
            ITool* cc2 = reg.createTool("CoordinateCalibration");
            cc2->setInstanceName("坐标校准-透视");
            cc2->setProperty("model", 1);     // 透视
            cc2->setProperty("srcPoints", srcText2);
            cc2->setProperty("dstPoints", dstText2);
            cc2->setProperty("queryX", 100.0);
            cc2->setProperty("queryY", 100.0);
            CHECK(cc2->execute(ctx), "坐标校准-透视: 执行成功");
            const double rms2 = cc2->resultData().value("rms").toDouble();
            const double px = cc2->resultData().value("resultX").toDouble();
            const double py = cc2->resultData().value("resultY").toDouble();
            CHECK(rms2 < 0.01, QString("坐标校准-透视: rms=%.4f<0.01").arg(rms2).toLocal8Bit().constData());
            // 查询点(100,100)变换应与正算一致
            const double expW = h20 * 100.0 + h21 * 100.0 + h22;
            const double expX = (h00 * 100.0 + h01 * 100.0 + h02) / expW;
            const double expY = (h10 * 100.0 + h11 * 100.0 + h12) / expW;
            CHECK(std::fabs(px - expX) < 0.01,
                  QString("坐标校准-透视: 变换X=%.3f 期望%.3f").arg(px).arg(expX).toLocal8Bit().constData());
            CHECK(std::fabs(py - expY) < 0.01,
                  QString("坐标校准-透视: 变换Y=%.3f 期望%.3f").arg(py).arg(expY).toLocal8Bit().constData());
            // 点数不足应NG
            ITool* cc3 = reg.createTool("CoordinateCalibration");
            cc3->setProperty("model", 0);
            cc3->setProperty("srcPoints", "1,1;2,2");   // 仿射需≥3
            cc3->setProperty("dstPoints", "5,5;6,6");
            CHECK(!cc3->execute(ctx), "坐标校准: 点数不足应NG");
            delete cc3;
            delete cc2;
        }
    }

    // ---- 12. 手眼标定: calibrateHandEye 求解相机→末端矩阵 X ----
    // 物理一致合成: 真值 X(cam2gripper) + 固定 C(base→target) + 随机机器人位姿 A(gripper2base),
    // 由链 A·X·B = C 反推 B(target2cam), 反标定应恢复 X
    {
        // 真值 X: 绕Z转30° + 平移(10,-20,50)
        const double angX = 30.0 * M_PI / 180.0;
        cv::Mat rvecX = (cv::Mat_<double>(3, 1) << 0, 0, angX);
        cv::Mat Rg0;
        cv::Rodrigues(rvecX, Rg0);
        cv::Mat Xm = cv::Mat::eye(4, 4, CV_64F);
        Rg0.copyTo(Xm(cv::Rect(0, 0, 3, 3)));
        Xm.at<double>(0, 3) = 10.0; Xm.at<double>(1, 3) = -20.0; Xm.at<double>(2, 3) = 50.0;
        cv::Mat Xinv = Xm.inv();
        // 固定 C = base→target (标定板固定在基座前方)
        cv::Mat rvecC = (cv::Mat_<double>(3, 1) << 0.1, -0.05, 0.2);
        cv::Mat RC;
        cv::Rodrigues(rvecC, RC);
        cv::Mat Cm = cv::Mat::eye(4, 4, CV_64F);
        RC.copyTo(Cm(cv::Rect(0, 0, 3, 3)));
        Cm.at<double>(0, 3) = 100.0; Cm.at<double>(1, 3) = 0.0; Cm.at<double>(2, 3) = 200.0;

        // 8 组机器人位姿 A_i (gripper2base): 大旋转 + 较大平移
        const double as[8][3] = {
            {0.8, 0.2, -0.3}, {-0.7, 0.9, 0.4}, {1.1, -0.4, 0.6}, {0.3, -1.0, 0.5},
            {-0.6, 0.5, -0.9}, {0.9, 0.7, 0.2}, {-1.2, 0.3, -0.4}, {0.5, -0.6, 0.8}};
        const double at[8][3] = {
            {300, 50, 220}, {350, 120, 180}, {270, -80, 240}, {330, 140, 200},
            {290, -60, 230}, {360, 80, 190}, {310, -30, 250}, {340, 60, 210}};
        QString rBaseText, tBaseText, rCamText, tCamText;
        for (int i = 0; i < 8; ++i) {
            cv::Mat rvecA = (cv::Mat_<double>(3, 1) << as[i][0], as[i][1], as[i][2]);
            cv::Mat R;
            cv::Rodrigues(rvecA, R);
            cv::Mat A = cv::Mat::eye(4, 4, CV_64F);
            R.copyTo(A(cv::Rect(0, 0, 3, 3)));
            A.at<double>(0, 3) = at[i][0]; A.at<double>(1, 3) = at[i][1]; A.at<double>(2, 3) = at[i][2];
            // B = Xinv · A⁻¹ · C  (target2cam)
            const cv::Mat B = Xinv * A.inv() * Cm;
            cv::Mat rB;
            cv::Rodrigues(B(cv::Rect(0, 0, 3, 3)), rB);
            if (i) {
                rBaseText += ";"; tBaseText += ";"; rCamText += ";"; tCamText += ";";
            }
            rBaseText += QString("%1,%2,%3").arg(as[i][0]).arg(as[i][1]).arg(as[i][2]);
            tBaseText += QString("%1,%2,%3").arg(at[i][0]).arg(at[i][1]).arg(at[i][2]);
            rCamText += QString("%1,%2,%3").arg(rB.at<double>(0)).arg(rB.at<double>(1)).arg(rB.at<double>(2));
            tCamText += QString("%1,%2,%3").arg(B.at<double>(0, 3)).arg(B.at<double>(1, 3)).arg(B.at<double>(2, 3));
        }

        ToolContext ctx;
        ITool* he = reg.createTool("HandEyeCalibration");
        he->setInstanceName("手眼标定");
        he->setProperty("rBase", rBaseText);
        he->setProperty("tBase", tBaseText);
        he->setProperty("rCam", rCamText);
        he->setProperty("tCam", tCamText);
        he->setProperty("method", 0);     // Tsai
        CHECK(he->execute(ctx), "手眼标定: 执行成功");
        if (he->status() == VisionInspector::ToolStatus::OK) {
            const QVariantList rv = he->resultData().value("handeyeRvec").toList();
            const QVariantList tv = he->resultData().value("handeyeTvec").toList();
            CHECK(rv.size() == 3 && tv.size() == 3, "手眼标定: rvec/tvec输出");
            if (rv.size() == 3 && tv.size() == 3) {
                // 恢复的旋转向量应≈(0,0,0.5236), 平移≈(10,-20,50)
                const double r3 = rv[2].toDouble();
                const double t0 = tv[0].toDouble(), t1 = tv[1].toDouble(), t2 = tv[2].toDouble();
                CHECK(std::fabs(r3 - angX) < 0.05,
                      QString("手眼标定: rvec.z=%.3f 期望%.3f").arg(r3).arg(angX).toLocal8Bit().constData());
                CHECK(std::fabs(rv[0].toDouble()) < 0.05 && std::fabs(rv[1].toDouble()) < 0.05,
                      "手眼标定: rvec.x/y≈0");
                CHECK(std::fabs(t0 - 10.0) < 1.0 && std::fabs(t1 + 20.0) < 1.0 && std::fabs(t2 - 50.0) < 1.0,
                      QString("手眼标定: tvec=(%.1f,%.1f,%.1f) 期望(10,-20,50)")
                          .arg(t0).arg(t1).arg(t2).toLocal8Bit().constData());
            }
            const double rms = he->resultData().value("handeyeRms").toDouble();
            // 合成数据自洽, 闭环残差应很小
            CHECK(rms < 0.5, QString("手眼标定: 旋转残差%.3f°<0.5").arg(rms).toLocal8Bit().constData());
            const int used = he->resultData().value("usedPoses").toInt();
            CHECK(used == 8, QString("手眼标定: 使用位姿%1").arg(used).toLocal8Bit().constData());
            // 上下文写入校验
            const double ctxTx = ctx.getDouble("handeye_tx", 0);
            CHECK(std::fabs(ctxTx - tv[0].toDouble()) < 1e-6, "手眼标定: 上下文tx一致");
        }
        delete he;

        // 点数不足应NG
        ITool* he2 = reg.createTool("HandEyeCalibration");
        he2->setProperty("rBase", "0,0,0.1;0,0,0.2");
        he2->setProperty("tBase", "100,0,0;200,0,0");
        he2->setProperty("rCam", "0,0,0.1;0,0,0.2");
        he2->setProperty("tCam", "50,0,0;150,0,0");
        CHECK(!he2->execute(ctx), "手眼标定: 位姿不足应NG");
        delete he2;
    }

    // ---- 13. 系统时间: 时间戳 + 耗时计时 (对标 CKVision 计算时间/系统时间) ----
    {
        ToolContext ctx;
        ITool* st = reg.createTool("SystemTime");
        st->setInstanceName("系统时间");
        // 模式0 时间戳: 当前时间合理(unix毫秒>1.7e12即2023年后) + 日期时间非空
        st->setProperty("mode", 0);
        CHECK(st->execute(ctx), "系统时间-时间戳: 执行成功");
        const qint64 unixMs = st->resultData().value("unixMs").toLongLong();
        CHECK(unixMs > 1700000000000LL,
              QString("系统时间-时间戳: unixMs=%1合理").arg(unixMs).toLocal8Bit().constData());
        CHECK(!st->resultData().value("datetime").toString().isEmpty(), "系统时间-时间戳: datetime非空");

        // 模式1 耗时计时: 多次执行 elapsedMs 递增 + runCount 递增
        st->setProperty("mode", 1);
        CHECK(st->execute(ctx), "系统时间-耗时: 第1次执行成功");
        const double e1 = st->resultData().value("elapsedMs").toDouble();
        CHECK(e1 >= 0.0, QString("系统时间-耗时: 首帧elapsedMs=%1≥0").arg(e1).toLocal8Bit().constData());
        const int rc1 = st->resultData().value("runCount").toInt();
        CHECK(rc1 == 1, QString("系统时间-耗时: runCount=%1").arg(rc1).toLocal8Bit().constData());
        // 第二次执行后 elapsedMs 应增大(单调非减), runCount=2
        CHECK(st->execute(ctx), "系统时间-耗时: 第2次执行成功");
        const double e2 = st->resultData().value("elapsedMs").toDouble();
        const int rc2 = st->resultData().value("runCount").toInt();
        CHECK(rc2 == 2, QString("系统时间-耗时: runCount=%1").arg(rc2).toLocal8Bit().constData());
        CHECK(e2 >= e1, QString("系统时间-耗时: elapsedMs单调(%1→%2)").arg(e1).arg(e2).toLocal8Bit().constData());
        delete st;
    }

    // ---- 14. 全局变量: SetVariable写入全局 + GetVariable读取 (§2.7 补齐) ----
    {
        GlobalVariables gv;
        ToolContext ctx;
        ctx.setGlobalVariables(&gv);

        // SetVariable scope=1 写入全局
        ITool* sv = reg.createTool("SetVariable");
        sv->setProperty("varName", "stationNo");
        sv->setProperty("varType", 0);       // 浮点
        sv->setProperty("doubleValue", 3.0);
        sv->setProperty("scope", 1);         // 全局变量
        CHECK(sv->execute(ctx), "设置变量-全局: 执行成功");
        CHECK(std::fabs(gv.getDouble("stationNo") - 3.0) < 1e-9,
              "设置变量-全局: 全局变量已写入");
        CHECK(sv->resultData().value("scope").toInt() == 1, "设置变量-全局: 输出scope=1");
        delete sv;

        // GetVariable 读取全局
        ITool* gv2 = reg.createTool("GetVariable");
        gv2->setProperty("varName", "stationNo");
        CHECK(gv2->execute(ctx), "获取全局变量: 执行成功");
        CHECK(std::fabs(gv2->resultData().value("value").toDouble() - 3.0) < 1e-9,
              "获取全局变量: 值=3.0");
        CHECK(gv2->resultData().value("found").toBool(), "获取全局变量: found=true");
        // 不存在的变量 found=false
        ITool* gv3 = reg.createTool("GetVariable");
        gv3->setProperty("varName", "notExistVar");
        CHECK(gv3->execute(ctx), "获取全局变量: 不存在时执行成功");
        CHECK(!gv3->resultData().value("found").toBool(), "获取全局变量: 不存在found=false");
        // 空变量名NG
        ITool* gv4 = reg.createTool("GetVariable");
        gv4->setProperty("varName", "");
        CHECK(!gv4->execute(ctx), "获取全局变量: 空变量名NG");
        delete gv4;
        delete gv3;
        delete gv2;

        // SetVariable scope=0 默认写当前流程 context (不污染全局)
        ITool* sv2 = reg.createTool("SetVariable");
        sv2->setProperty("varName", "localOnly");
        sv2->setProperty("varType", 1);      // 整型
        sv2->setProperty("intValue", 7);
        sv2->setProperty("scope", 0);
        CHECK(sv2->execute(ctx), "设置变量-当前流程: 执行成功");
        CHECK(!gv.has("localOnly"), "设置变量-当前流程: 未写入全局");
        CHECK(ctx.getDouble("localOnly", -1) == 7.0, "设置变量-当前流程: 写入context");
        delete sv2;
    }

    // ---- 15. 图像去畸变: 合成畸变点阵 → 去畸变 → 点阵恢复 (§3.3 闭环) ----
    {
        ToolContext ctx;
        const int W = 640, H = 480;
        const double fx = 800.0, fy = 800.0, cx = 320.0, cy = 240.0;
        const double k1 = 0.4, k2 = 0.05, p1 = 0.01, p2 = -0.02, k3 = 0.0;

        // 理想点阵: 网格点中心
        std::vector<cv::Point2d> dots;
        for (int y = 120; y <= 380; y += 100)
            for (int x = 120; x <= 520; x += 100)
                dots.push_back(cv::Point2d(x, y));

        // 解析前向畸变: 把理想点中心按畸变模型投影到畸变像素位置
        //   xd = xn*(1+k1 r2+k2 r4+k3 r6) + 2p1 xn yn + p2(r2+2xn^2)
        //   yd = yn*(1+k1 r2+k2 r4+k3 r6) + p1(r2+2yn^2) + 2p2 xn yn
        cv::Mat distorted(H, W, CV_8UC1, cv::Scalar(0));
        int moved = 0;
        for (const auto& d : dots) {
            const double xn = (d.x - cx) / fx, yn = (d.y - cy) / fy;
            const double r2 = xn * xn + yn * yn, r4 = r2 * r2, r6 = r4 * r2;
            const double radial = 1 + k1 * r2 + k2 * r4 + k3 * r6;
            const double xd = xn * radial + 2 * p1 * xn * yn + p2 * (r2 + 2 * xn * xn);
            const double yd = yn * radial + p1 * (r2 + 2 * yn * yn) + 2 * p2 * xn * yn;
            const double u = fx * xd + cx, v = fy * yd + cy;
            cv::circle(distorted, cv::Point2d(u, v), 5, cv::Scalar(255), -1);
            // 统计被明显推离理想位置的点 (畸变生效: 角点/边缘点位移应显著)
            if (std::hypot(u - d.x, v - d.y) > 3.0) ++moved;
        }
        CHECK(moved >= 2,
              QString("图像去畸变: 畸变生效(%1/15点位移>3px)").arg(moved).toLocal8Bit().constData());

        // 手动内参路径去畸变
        ctx.setCurrentImage(std::make_shared<CvImage>(distorted));
        ITool* ud = reg.createTool("ImageUndistort");
        ud->setProperty("useContextCalib", false);
        ud->setProperty("fx", fx);  ud->setProperty("fy", fy);
        ud->setProperty("cx", cx);  ud->setProperty("cy", cy);
        ud->setProperty("k1", k1);  ud->setProperty("k2", k2);
        ud->setProperty("p1", p1);  ud->setProperty("p2", p2);
        ud->setProperty("k3", k3);
        CHECK(ud->execute(ctx), "图像去畸变-手动: 执行成功");
        CHECK(ud->resultData().value("undistorted").toBool(), "图像去畸变-手动: undistorted=true");
        CHECK(ud->resultData().value("source").toInt() == 1, "图像去畸变-手动: source=1(手动)");
        CvImagePtr out = ctx.currentImage();
        CHECK(out && !out->empty(), "图像去畸变-手动: 有输出图像");

        // 点阵恢复: 每个理想中心±8px窗口内最大灰度>200 (去畸变后点回到原位)
        int recovered = 0;
        for (const auto& d : dots) {
            double mx = 0;
            for (int dy = -8; dy <= 8; ++dy)
                for (int dx = -8; dx <= 8; ++dx) {
                    const int px = (int)std::lround(d.x) + dx, py = (int)std::lround(d.y) + dy;
                    if (px >= 0 && px < W && py >= 0 && py < H)
                        mx = std::max(mx, (double)out->at<uchar>(py, px));
                }
            if (mx > 200) ++recovered;
        }
        CHECK(recovered >= (int)dots.size() - 1,
              QString("图像去畸变-手动: 点阵恢复%1/%2").arg(recovered).arg(dots.size()).toLocal8Bit().constData());
        delete ud;

        // 上下文标定结果路径 (写入 calibration_camera_* 模拟相机标定输出)
        ToolContext ctx2;
        ctx2.setCurrentImage(std::make_shared<CvImage>(distorted));
        ctx2.setData("calibration_camera_fx", fx);
        ctx2.setData("calibration_camera_fy", fy);
        ctx2.setData("calibration_camera_cx", cx);
        ctx2.setData("calibration_camera_cy", cy);
        ctx2.setData("calibration_camera_dist", QVariantList{k1, k2, p1, p2, k3});
        ctx2.setData("calibration_reprojection_error", 0.3);
        ITool* ud2 = reg.createTool("ImageUndistort");
        ud2->setProperty("useContextCalib", true);
        CHECK(ud2->execute(ctx2), "图像去畸变-标定: 执行成功");
        CHECK(ud2->resultData().value("source").toInt() == 0, "图像去畸变-标定: source=0(上下文)");
        CHECK(ud2->resultData().value("reprojError").toDouble() == 0.3, "图像去畸变-标定: 透传重投影误差");
        int recovered2 = 0;
        CvImagePtr out2 = ctx2.currentImage();
        for (const auto& d : dots) {
            double mx = 0;
            for (int dy = -8; dy <= 8; ++dy)
                for (int dx = -8; dx <= 8; ++dx) {
                    const int px = (int)std::lround(d.x) + dx, py = (int)std::lround(d.y) + dy;
                    if (px >= 0 && px < W && py >= 0 && py < H)
                        mx = std::max(mx, (double)out2->at<uchar>(py, px));
                }
            if (mx > 200) ++recovered2;
        }
        CHECK(recovered2 >= (int)dots.size() - 1,
              QString("图像去畸变-标定: 点阵恢复%1/%2").arg(recovered2).arg(dots.size()).toLocal8Bit().constData());
        delete ud2;

        // 无图像输入NG
        ToolContext ctx3;
        ITool* ud3 = reg.createTool("ImageUndistort");
        CHECK(!ud3->execute(ctx3), "图像去畸变: 无输入图像NG");
        delete ud3;
    }

    // ---- 16. 文本三件套: 生成/分解/比较文本 (§2.7 补齐) ----
    {
        ToolContext ctx;
        ctx.setData("circle.radius", 128.35);
        ctx.setData("message", "OK");

        // 生成文本: 模板占位符替换 (直接变量+上下文+全局)
        ITool* gt = reg.createTool("GenerateText");
        gt->setProperty("template", "半径={1} 消息={circle.radius}");
        gt->setProperty("var1", "12.3");
        CHECK(gt->execute(ctx), "生成文本: 执行成功");
        CHECK(gt->resultData().value("text").toString() == "半径=12.3 消息=128.35",
              "生成文本: 直接变量+上下文替换");
        CHECK(gt->resultData().value("replacedCount").toInt() == 2, "生成文本: 替换2处");
        delete gt;

        // 生成文本: 全局变量回退 + 缺失键保留
        ITool* gt2 = reg.createTool("GenerateText");
        gt2->setProperty("template", "{message}-{missing}");
        CHECK(gt2->execute(ctx), "生成文本: 执行成功");
        CHECK(gt2->resultData().value("text").toString() == "OK-{missing}",
              "生成文本: 上下文替换, 缺失键保留");
        delete gt2;

        // 分解文本: 逗号分隔
        ITool* st = reg.createTool("SplitText");
        st->setProperty("inputText", "A,B,C");
        st->setProperty("delimiter", 0);       // 逗号
        st->setProperty("selectIndex", -1);
        CHECK(st->execute(ctx), "分解文本: 执行成功");
        CHECK(st->resultData().value("partCount").toInt() == 3, "分解文本: 3段");
        CHECK(st->resultData().value("part0").toString() == "A", "分解文本: part0=A");
        CHECK(st->resultData().value("part2").toString() == "C", "分解文本: part2=C");
        // 取第1段(0基)
        st->setProperty("selectIndex", 1);
        CHECK(st->execute(ctx), "分解文本: 执行成功");
        CHECK(st->resultData().value("selected").toString() == "B", "分解文本: selected=B");
        delete st;

        // 分解文本: 自定义分隔符
        ITool* st2 = reg.createTool("SplitText");
        st2->setProperty("inputText", "1|2|3");
        st2->setProperty("delimiter", 4);      // 自定义
        st2->setProperty("customDelimiter", "|");
        st2->setProperty("selectIndex", 2);
        CHECK(st2->execute(ctx), "分解文本: 自定义分隔符执行成功");
        CHECK(st2->resultData().value("selected").toString() == "3", "分解文本: 自定义分隔符selected=3");
        delete st2;

        // 比较文本: 区分大小写等于
        ITool* ct = reg.createTool("CompareText");
        ct->setProperty("textA", "ABC");
        ct->setProperty("textB", "abc");
        ct->setProperty("compareMode", 0);     // 等于
        ct->setProperty("caseSensitive", true);
        CHECK(ct->execute(ctx), "比较文本: 执行成功");
        CHECK(!ct->resultData().value("equal").toBool(), "比较文本: 区分大小写不等");
        CHECK(ct->status() == ToolStatus::NG, "比较文本: 不匹配状态NG");
        // 不区分大小写
        ct->setProperty("caseSensitive", false);
        CHECK(ct->execute(ctx), "比较文本: 执行成功");
        CHECK(ct->resultData().value("equal").toBool(), "比较文本: 不区分大小写相等");
        delete ct;

        // 比较文本: 包含 + 正则
        ITool* ct2 = reg.createTool("CompareText");
        ct2->setProperty("textA", "Hello World");
        ct2->setProperty("textB", "World");
        ct2->setProperty("compareMode", 2);    // 包含
        CHECK(ct2->execute(ctx), "比较文本: 包含执行成功");
        CHECK(ct2->resultData().value("matched").toBool(), "比较文本: 包含匹配");
        ct2->setProperty("compareMode", 5);    // 正则
        ct2->setProperty("textA", "ABC123XYZ");
        ct2->setProperty("textB", "^A.*Z$");
        CHECK(ct2->execute(ctx), "比较文本: 正则执行成功");
        CHECK(ct2->resultData().value("matched").toBool(), "比较文本: 正则匹配");
        // 正则不匹配
        ct2->setProperty("textB", "^A[0-9]+$");
        CHECK(ct2->execute(ctx), "比较文本: 正则不匹配执行成功");
        CHECK(!ct2->resultData().value("matched").toBool(), "比较文本: 正则不匹配");
        delete ct2;
    }

    // ---- 17. 线到线: 平行线间距 / 垂直距离 / 平行判定 (§2.4 补齐) ----
    {
        ToolContext ctx;
        ITool* ll = reg.createTool("LineToLine");

        // 平行水平线: y=100 与 y=150, 间距50
        ll->setProperty("line1CenterX", "100");
        ll->setProperty("line1CenterY", "100");
        ll->setProperty("line1Angle", "0");
        ll->setProperty("line2CenterX", "200");
        ll->setProperty("line2CenterY", "150");
        ll->setProperty("line2Angle", "0");
        CHECK(ll->execute(ctx), "线到线: 平行线执行成功");
        CHECK(std::fabs(ll->resultData().value("distance").toDouble() - 50.0) < 1e-6,
              "线到线: 平行线间距50");
        CHECK(ll->resultData().value("parallel").toBool(), "线到线: 平行判定true");
        CHECK(std::fabs(ll->resultData().value("angleBetween").toDouble()) < 1e-6,
              "线到线: 夹角0");

        // 垂直相交: 线1水平过(100,100), 线2竖直过(300,300) → 线1中心到线2距离200, 夹角90
        ll->setProperty("line1CenterX", "100");
        ll->setProperty("line1CenterY", "100");
        ll->setProperty("line1Angle", "0");
        ll->setProperty("line2CenterX", "300");
        ll->setProperty("line2CenterY", "300");
        ll->setProperty("line2Angle", "90");
        CHECK(ll->execute(ctx), "线到线: 垂直执行成功");
        CHECK(std::fabs(ll->resultData().value("distance").toDouble() - 200.0) < 1e-6,
              "线到线: 线1中心到竖直线2距离200");
        CHECK(!ll->resultData().value("parallel").toBool(), "线到线: 垂直非平行");
        CHECK(std::fabs(ll->resultData().value("angleBetween").toDouble() - 90.0) < 1e-6,
              "线到线: 夹角90");

        // 45°斜线: 线1水平过(0,0), 线2 45°过(10,0) (即 y=x-10) → 线1中心(0,0)到线2垂距10/sqrt2≈7.07
        ll->setProperty("line1CenterX", "0");
        ll->setProperty("line1CenterY", "0");
        ll->setProperty("line1Angle", "0");
        ll->setProperty("line2CenterX", "10");
        ll->setProperty("line2CenterY", "0");
        ll->setProperty("line2Angle", "45");
        CHECK(ll->execute(ctx), "线到线: 斜线执行成功");
        CHECK(std::fabs(ll->resultData().value("distance").toDouble() - 10.0 / std::sqrt(2.0)) < 1e-6,
              "线到线: 斜线垂距7.07");
        CHECK(std::fabs(ll->resultData().value("angleBetween").toDouble() - 45.0) < 1e-6,
              "线到线: 夹角45");
        delete ll;
    }

    // ---- 18. 延伸点/旋转点: 几何点运算 (§2.4 补齐) ----
    {
        ToolContext ctx;

        // 延伸点: 从(100,100)沿90°(竖直向下, 图像Y轴向下)延伸50 → (100,150)
        ITool* ep = reg.createTool("ExtendPoint");
        ep->setProperty("startX", "100");
        ep->setProperty("startY", "100");
        ep->setProperty("angleDeg", "90");
        ep->setProperty("length", "50");
        CHECK(ep->execute(ctx), "延伸点: 执行成功");
        CHECK(std::fabs(ep->resultData().value("endX").toDouble() - 100.0) < 1e-9, "延伸点: endX=100");
        CHECK(std::fabs(ep->resultData().value("endY").toDouble() - 150.0) < 1e-9, "延伸点: endY=150");
        // 沿0°(水平右)延伸25 → (125,100)
        ep->setProperty("angleDeg", "0");
        ep->setProperty("length", "25");
        CHECK(ep->execute(ctx), "延伸点: 水平执行成功");
        CHECK(std::fabs(ep->resultData().value("endX").toDouble() - 125.0) < 1e-9, "延伸点: 水平endX=125");
        CHECK(std::fabs(ep->resultData().value("endY").toDouble() - 100.0) < 1e-9, "延伸点: 水平endY=100");
        delete ep;

        // 旋转点: (10,0)绕原点(0,0)旋转90° → (0,10)
        ITool* rp = reg.createTool("RotatePoint");
        rp->setProperty("pointX", "10");
        rp->setProperty("pointY", "0");
        rp->setProperty("centerX", "0");
        rp->setProperty("centerY", "0");
        rp->setProperty("angleDeg", "90");
        CHECK(rp->execute(ctx), "旋转点: 执行成功");
        CHECK(std::fabs(rp->resultData().value("resultX").toDouble() - 0.0) < 1e-9, "旋转点: resultX=0");
        CHECK(std::fabs(rp->resultData().value("resultY").toDouble() - 10.0) < 1e-9, "旋转点: resultY=10");
        // 绕中心(100,100)旋转(100,100) → 保持不变
        rp->setProperty("pointX", "100");
        rp->setProperty("pointY", "100");
        rp->setProperty("centerX", "100");
        rp->setProperty("centerY", "100");
        rp->setProperty("angleDeg", "45");
        CHECK(rp->execute(ctx), "旋转点: 中心点执行成功");
        CHECK(std::fabs(rp->resultData().value("resultX").toDouble() - 100.0) < 1e-9, "旋转点: 中心点resultX=100");
        CHECK(std::fabs(rp->resultData().value("resultY").toDouble() - 100.0) < 1e-9, "旋转点: 中心点resultY=100");
        delete rp;
    }

    // ---- 19. 数据队列: 入队/出队/窥视/清空/长度/空队列默认值 (§P0-9 补齐) ----
    {
        GlobalVariables gv;
        ToolContext ctx;
        ctx.setGlobalVariables(&gv);
        // 用独立队列名, 避免与其它测试冲突
        ITool* dq = reg.createTool("DataQueue");
        dq->setProperty("queueName", "testQ1");

        // 空队列窥视 → 默认值
        dq->setProperty("operation", 2);  // 窥视
        dq->setProperty("defaultValue", "EMPTY");
        CHECK(dq->execute(ctx), "数据队列: 空窥视执行成功");
        CHECK(dq->resultData().value("value").toString() == "EMPTY", "数据队列: 空队列窥视默认值");

        // 入队 3 个值 (FIFO)
        dq->setProperty("operation", 0);  // 入队
        dq->setProperty("value", "10");
        CHECK(dq->execute(ctx), "数据队列: 入队1");
        CHECK(dq->resultData().value("queueLength").toInt() == 1, "数据队列: 入队1后长度1");
        dq->setProperty("value", "20");
        CHECK(dq->execute(ctx), "数据队列: 入队2");
        dq->setProperty("value", "30");
        CHECK(dq->execute(ctx), "数据队列: 入队3");
        CHECK(dq->resultData().value("queueLength").toInt() == 3, "数据队列: 入队3后长度3");

        // 窥视队首 (不移除)
        dq->setProperty("operation", 2);
        CHECK(dq->execute(ctx), "数据队列: 窥视");
        CHECK(dq->resultData().value("value").toString() == "10", "数据队列: 窥视队首=10");
        CHECK(dq->resultData().value("queueLength").toInt() == 3, "数据队列: 窥视不移除长度3");

        // 出队 FIFO
        dq->setProperty("operation", 1);
        CHECK(dq->execute(ctx), "数据队列: 出队1");
        CHECK(dq->resultData().value("value").toString() == "10", "数据队列: 出队1=10");
        CHECK(dq->execute(ctx), "数据队列: 出队2");
        CHECK(dq->resultData().value("value").toString() == "20", "数据队列: 出队2=20");
        CHECK(dq->resultData().value("queueLength").toInt() == 1, "数据队列: 出队2后长度1");

        // 跨执行持久: 新 ToolContext 仍能看到剩余队列 (通过同一 GlobalVariables)
        ToolContext ctx2;
        ctx2.setGlobalVariables(&gv);
        ITool* dq2 = reg.createTool("DataQueue");
        dq2->setProperty("queueName", "testQ1");
        dq2->setProperty("operation", 4);  // 长度
        CHECK(dq2->execute(ctx2), "数据队列: 跨执行长度查询");
        CHECK(dq2->resultData().value("queueLength").toInt() == 1, "数据队列: 跨执行剩余长度1");
        // 出队剩余
        dq2->setProperty("operation", 1);
        dq2->setProperty("defaultValue", "EMPTY");
        CHECK(dq2->execute(ctx2), "数据队列: 跨执行出队");
        CHECK(dq2->resultData().value("value").toString() == "30", "数据队列: 跨执行出队=30");

        // 清空
        dq2->setProperty("operation", 3);
        CHECK(dq2->execute(ctx2), "数据队列: 清空");
        CHECK(dq2->resultData().value("queueLength").toInt() == 0, "数据队列: 清空后长度0");
        // 清空后再出队 → 空默认值
        dq2->setProperty("operation", 1);
        CHECK(dq2->execute(ctx2), "数据队列: 清空后出队");
        CHECK(dq2->resultData().value("value").toString() == "EMPTY", "数据队列: 清空后出队默认值");

        // 空队列名 → NG
        ITool* dq3 = reg.createTool("DataQueue");
        dq3->setProperty("queueName", "");
        CHECK(!dq3->execute(ctx2), "数据队列: 空队列名NG");
        delete dq; delete dq2; delete dq3;
    }

    // ---- 测试20: 通讯四件套 (P0-10) ----
    std::printf("测试20: 播放声音/写入文本/文件监测/TCP收发\n");
    {
        // 20.1 播放声音 (系统提示音; 不实际出声只验证执行路径)
        ITool* ps = reg.createTool("PlaySound");
        ToolContext ctx20;
        ps->setProperty("playMode", 0);
        CHECK(ps->execute(ctx20), "播放声音: 系统提示音执行");
        CHECK(ps->resultData().value("played").toBool(), "播放声音: played=true");
        ps->setProperty("playMode", 1);
        ps->setProperty("soundPath", "");
        CHECK(ps->execute(ctx20), "播放声音: 空路径wav执行(不崩溃)");
        delete ps;

        // 20.2 写入文本 (临时目录, 追加/覆盖/内容验证)
        const QString tmpDir = QDir::temp().filePath("lvw_test_wt");
        QDir().mkpath(tmpDir);
        ITool* wt = reg.createTool("WriteText");
        wt->setProperty("directory", tmpDir);
        wt->setProperty("fileName", "t20");
        wt->setProperty("extension", "csv");
        wt->setProperty("content", "10,20,30");
        wt->setProperty("append", true);
        CHECK(wt->execute(ctx20), "写入文本: 追加写入");
        const QString f20 = QFileInfo(wt->resultData().value("filePath").toString()).absoluteFilePath();
        CHECK(QFileInfo::exists(f20), "写入文本: 文件已生成");
        QFile rf(f20);
        CHECK(rf.open(QIODevice::ReadOnly | QIODevice::Text), "写入文本: 打开读取");
        const QString firstRead = QString::fromUtf8(rf.readAll());
        rf.close();
        CHECK(firstRead.contains("10,20,30"), "写入文本: 内容正确");
        // 覆盖模式 → 只保留最新
        wt->setProperty("append", false);
        wt->setProperty("content", "99");
        CHECK(wt->execute(ctx20), "写入文本: 覆盖写入");
        QFile rf2(f20);
        CHECK(rf2.open(QIODevice::ReadOnly | QIODevice::Text), "写入文本: 覆盖后读取");
        const QString overRead = QString::fromUtf8(rf2.readAll());
        rf2.close();
        CHECK(!overRead.contains("10,20,30"), "写入文本: 覆盖后旧内容消失");
        CHECK(overRead.contains("99"), "写入文本: 覆盖后新内容存在");
        delete wt;
        QFile::remove(f20);
        QDir().rmdir(tmpDir);

        // 20.3 文件监测 (检查存在 / 删除 / 清理旧文件)
        const QString fwPath = QDir::temp().filePath("lvw_test_fw.sig");
        QFile::remove(fwPath);
        ITool* fw = reg.createTool("FileWatch");
        fw->setProperty("filePath", fwPath);
        fw->setProperty("watchMode", 0);  // 检查存在
        CHECK(fw->execute(ctx20), "文件监测: 检查存在执行");
        CHECK(fw->resultData().value("fileExists").toBool() == false, "文件监测: 初始不存在");
        QFile fwFile(fwPath);
        CHECK(fwFile.open(QIODevice::WriteOnly), "文件监测: 创建信号文件");
        fwFile.write("x");
        fwFile.close();
        CHECK(fw->execute(ctx20), "文件监测: 再检查");
        CHECK(fw->resultData().value("fileExists").toBool() == true, "文件监测: 已出现");
        fw->setProperty("watchMode", 1);  // 删除
        CHECK(fw->execute(ctx20), "文件监测: 删除执行");
        CHECK(!QFileInfo::exists(fwPath), "文件监测: 已删除");
        fw->setProperty("watchMode", 2);  // 清理旧文件 (空目录, 保留7天 → 不误删)
        fw->setProperty("filePath", QDir::temp().path());
        CHECK(fw->execute(ctx20), "文件监测: 清理旧文件执行");
        CHECK(fw->resultData().value("cleanedCount").toInt() >= 0, "文件监测: 清理计数合法");
        delete fw;

        // 20.4 TCP 收发 (本地回环: 起一个一次性 echo 服务器)
        static std::atomic<quint16> s_portSeed{23456};
        const quint16 tcpPort = s_portSeed.fetch_add(1) % 30000 + 20000;
        std::atomic<bool> echoDone{false};
        QThread* serverThread = QThread::create([&] {
            QTcpServer server;
            if (!server.listen(QHostAddress::LocalHost, tcpPort)) return;
            if (!server.waitForNewConnection(5000)) return;
            QTcpSocket* s = server.nextPendingConnection();
            if (!s) return;
            s->waitForReadyRead(5000);
            const QByteArray in = s->readAll();
            s->write(in);                 // echo
            s->waitForBytesWritten(3000);
            s->disconnectFromHost();
            delete s;
            echoDone.store(true, std::memory_order_relaxed);
        });
        serverThread->start();
        QThread::msleep(200);   // 等服务器就绪

        ITool* tc = reg.createTool("TcpData");
        tc->setProperty("host", "127.0.0.1");
        tc->setProperty("port", (int)tcpPort);
        tc->setProperty("operation", 2);   // 发送并接收
        tc->setProperty("sendData", "HELLO-LW");
        tc->setProperty("timeoutMs", 5000);
        CHECK(tc->execute(ctx20), "TCP收发: 发送并接收执行");
        CHECK(tc->resultData().value("received").toString() == "HELLO-LW", "TCP收发: 回显内容正确");
        CHECK(tc->resultData().value("bytesSent").toInt() == 8, "TCP收发: 发送字节数8");
        CHECK(echoDone.load(), "TCP收发: 服务器已完成echo");
        delete tc;

        // 20.5 TCP 收发: 拒绝连接 → NG
        ITool* tc2 = reg.createTool("TcpData");
        tc2->setProperty("host", "127.0.0.1");
        tc2->setProperty("port", 1);       // 未监听端口
        tc2->setProperty("operation", 0);  // 发送
        tc2->setProperty("timeoutMs", 500);
        CHECK(tc2->execute(ctx20), "TCP收发: 拒连执行");
        CHECK(tc2->resultData().value("connected").toBool() == false, "TCP收发: 拒连未连接");
        CHECK(tc2->status() == ToolStatus::NG, "TCP收发: 拒连状态NG");
        delete tc2;

        serverThread->quit();
        serverThread->wait(3000);
        delete serverThread;
    }

    // ---- 测试21: 斑点分类 (P1-13) ----
    std::printf("测试21: 斑点分类(BLOB特征分类)\n");
    {
        // 合成图: 黑底 + 白色圆(左上) + 白色矩形(右上) + 圆环(下, 带孔)
        cv::Mat blobImg(240, 240, CV_8UC1, cv::Scalar(0));
        cv::circle(blobImg, cv::Point(70, 70), 30, cv::Scalar(255), -1);        // 圆 r=30
        cv::rectangle(blobImg, cv::Rect(140, 40, 60, 20), cv::Scalar(255), -1); // 矩形 60x20
        cv::circle(blobImg, cv::Point(80, 160), 30, cv::Scalar(255), -1);       // 环外圆
        cv::circle(blobImg, cv::Point(80, 160), 12, cv::Scalar(0), -1);         // 环内孔 r=12

        ToolContext ctx21;
        ctx21.setCurrentImage(std::make_shared<CvImage>(blobImg));
        ITool* bc = reg.createTool("BlobClassify");
        bc->setProperty("autoThreshold", false);
        bc->setProperty("threshold", 127);
        bc->setProperty("detectionType", 1);  // 白色目标
        bc->setProperty("useAreaFilter", true);
        bc->setProperty("minArea", 10);

        // 不启用特征过滤 → 3 个斑点 (圆/矩形/圆环)
        CHECK(bc->execute(ctx21), "斑点分类: 混合图执行");
        CHECK(bc->resultData().value("blobCount").toInt() == 3, "斑点分类: 3个斑点");

        // 圆度: 圆与圆环都≈1 (圆环外轮廓仍是圆)
        bc->setProperty("useCircularityFilter", true);
        bc->setProperty("minCircularity", 0.9);
        CHECK(bc->execute(ctx21), "斑点分类: 圆度过滤执行");
        CHECK(bc->resultData().value("blobCount").toInt() == 2, "斑点分类: 圆度>0.9只留圆+环");
        CHECK(bc->resultData().value("mainCircularity").toDouble() > 0.9, "斑点分类: 主斑圆度>0.9");

        // 长宽比: 只留矩形 (60x20 → 长宽比≈3)
        bc->setProperty("useCircularityFilter", false);
        bc->setProperty("useAspectFilter", true);
        bc->setProperty("minAspectRatio", 2.0);
        bc->setProperty("maxAspectRatio", 10.0);
        CHECK(bc->execute(ctx21), "斑点分类: 长宽比过滤执行");
        CHECK(bc->resultData().value("blobCount").toInt() == 1, "斑点分类: 长宽比>2只留矩形");
        const double asp = bc->resultData().value("mainAspectRatio").toDouble();
        CHECK(asp > 2.5 && asp < 3.5, "斑点分类: 矩形长宽比≈3");

        // 孔数: 只留圆环 (1孔)
        bc->setProperty("useAspectFilter", false);
        bc->setProperty("useHoleFilter", true);
        bc->setProperty("minHoles", 1);
        bc->setProperty("maxHoles", 10);
        CHECK(bc->execute(ctx21), "斑点分类: 孔数过滤执行");
        CHECK(bc->resultData().value("blobCount").toInt() == 1, "斑点分类: 孔数>=1只留圆环");
        CHECK(bc->resultData().value("mainHoles").toInt() == 1, "斑点分类: 圆环孔数1");

        // 面积过滤: 圆环外圆面积≈π*30²≈2827 → 只留最大(圆环)
        bc->setProperty("useHoleFilter", false);
        bc->setProperty("useAreaFilter", true);
        bc->setProperty("minArea", 2000);
        bc->setProperty("maxArea", 4000);
        CHECK(bc->execute(ctx21), "斑点分类: 面积过滤执行");
        CHECK(bc->resultData().value("blobCount").toInt() == 2, "斑点分类: 面积2000~4000留圆+环");

        // 判定: 有效数不在 [minCount,maxCount] → NG
        bc->setProperty("useAreaFilter", false);
        bc->setProperty("minCount", 4);
        bc->setProperty("maxCount", 100);
        CHECK(!bc->execute(ctx21), "斑点分类: minCount=4(实际3)→NG");
        CHECK(bc->resultData().value("judgment").toString() == "NG", "斑点分类: 判定NG");
        CHECK(bc->status() == ToolStatus::NG, "斑点分类: 状态NG");
        bc->setProperty("minCount", 1);
        bc->setProperty("maxCount", 3);
        CHECK(bc->execute(ctx21), "斑点分类: minCount=1 maxCount=3→OK");
        CHECK(bc->resultData().value("judgment").toString() == "OK", "斑点分类: 判定OK");
        delete bc;
    }

    // ---- 测试22: 图像运算 + 存储图像 (P1-11) ----
    std::printf("测试22: 图像运算/存储图像\n");
    {
        // 两张 120x100 灰度图: A=200, B=60 (存入命名槽 imgA/imgB)
        cv::Mat imgA(100, 120, CV_8UC1, cv::Scalar(200));
        cv::Mat imgB(100, 120, CV_8UC1, cv::Scalar(60));
        ToolContext ctx22;
        ctx22.setCurrentImage(std::make_shared<CvImage>(imgA));
        ctx22.setImage("imgA", std::make_shared<CvImage>(imgA));
        ctx22.setImage("imgB", std::make_shared<CvImage>(imgB));

        ITool* io = reg.createTool("ImageOperation");
        io->setProperty("inputImage", "imgA");
        io->setProperty("inputImage2", "imgB");

        // 加 (饱和): 200+60=260→255
        io->setProperty("operation", 0);
        CHECK(io->execute(ctx22), "图像运算: 加执行");
        CvImagePtr sum = ctx22.currentImage();
        CHECK(sum && !sum->empty(), "图像运算: 加有输出图");
        CHECK(sum->at<uchar>(50, 60) == 255, "图像运算: 加饱和255");

        // 减: 200-60=140
        io->setProperty("operation", 1);
        CHECK(io->execute(ctx22), "图像运算: 减执行");
        CHECK(ctx22.currentImage()->at<uchar>(50, 60) == 140, "图像运算: 减140");

        // 差分: |60-140|=80 (输入2=当前图140)
        io->setProperty("operation", 2);
        io->setProperty("inputImage", "imgB");
        io->setProperty("inputImage2", "Current");
        CHECK(io->execute(ctx22), "图像运算: 差分执行");
        CHECK(ctx22.currentImage()->at<uchar>(50, 60) == 80, "图像运算: 差分|60-140|=80");

        // 平均: (200+60)/2=130
        io->setProperty("inputImage", "imgA");
        io->setProperty("inputImage2", "imgB");
        io->setProperty("operation", 8);
        CHECK(io->execute(ctx22), "图像运算: 平均执行");
        CHECK(ctx22.currentImage()->at<uchar>(50, 60) == 130, "图像运算: 平均130");

        // 与: 200&60 = 11001000 & 00111100 = 8
        io->setProperty("operation", 3);
        CHECK(io->execute(ctx22), "图像运算: 与执行");
        CHECK(ctx22.currentImage()->at<uchar>(50, 60) == (200 & 60), "图像运算: 与按位");

        // 异或: 200^60
        io->setProperty("operation", 5);
        CHECK(io->execute(ctx22), "图像运算: 异或执行");
        CHECK(ctx22.currentImage()->at<uchar>(50, 60) == (200 ^ 60), "图像运算: 异或按位");
        CHECK(ctx22.currentImage()->cols == 120 && ctx22.currentImage()->rows == 100,
              "图像运算: 输出尺寸正确");
        delete io;

        // 存储图像: PNG 到临时目录, 回读验证
        const QString saveDir = QDir::temp().filePath("lvw_test_save");
        QDir().mkpath(saveDir);
        ctx22.setCurrentImage(std::make_shared<CvImage>(imgA));
        ITool* si = reg.createTool("SaveImage");
        si->setProperty("directory", saveDir);
        si->setProperty("fileName", "t22");
        si->setProperty("format", 0);  // PNG
        CHECK(si->execute(ctx22), "存储图像: PNG执行");
        const QString savedPath = si->resultData().value("filePath").toString();
        CHECK(si->resultData().value("saved").toBool(), "存储图像: saved=true");
        CHECK(QFileInfo::exists(savedPath), "存储图像: 文件已生成");
        cv::Mat back = cv::imread(savedPath.toLocal8Bit().toStdString(), cv::IMREAD_GRAYSCALE);
        CHECK(!back.empty(), "存储图像: 回读成功");
        CHECK(back.cols == 120 && back.rows == 100, "存储图像: 回读尺寸一致");
        CHECK(back.at<uchar>(50, 60) == 200, "存储图像: 回读像素一致");
        // JPG 质量路径
        si->setProperty("format", 1);
        CHECK(si->execute(ctx22), "存储图像: JPG执行");
        CHECK(QFileInfo::exists(si->resultData().value("filePath").toString()), "存储图像: JPG文件生成");
        delete si;
        QDir(saveDir).removeRecursively();
    }

    std::printf("\n回归结果: %d项检查, 硬失败%d | 找圆%d/%d | 亚像素%d/%d | 最差半径误差%.2fpx\n",
                g_checks, g_failures, circleFinds, images.size(),
                subpixOk, images.size(), worstRadiusErr);
    return g_failures == 0 ? 0 : 1;
}
