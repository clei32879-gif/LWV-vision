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
#include <QCoreApplication>
#include <QDir>
#include <QFile>
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

    std::printf("\n回归结果: %d项检查, 硬失败%d | 找圆%d/%d | 亚像素%d/%d | 最差半径误差%.2fpx\n",
                g_checks, g_failures, circleFinds, images.size(),
                subpixOk, images.size(), worstRadiusErr);
    return g_failures == 0 ? 0 : 1;
}
