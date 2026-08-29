/**
 * @file testdata_gen.cpp
 * @brief 测试图片生成器 — 合成螺丝/缺陷样本, 供虚拟相机回放与算法回归
 *
 * 用法: testdata_gen [输出目录] [OK张数] [每类NG张数]
 * 默认输出 ./testdata/virtual_camera, OK 24张, NG每类6张
 *
 * 生成内容 (640x480 灰度):
 *   OK_01..N       : 完整外螺纹工件 (牙型相位旋转)
 *   NG_MISSING_XX  : 缺牙 (牙型出现缺口)
 *   NG_BURR_XX     : 毛刺 (牙顶亮点尖刺)
 *   NG_DAMAGED_XX  : 烂牙 (局部牙型模糊坍塌)
 *   NG_SLANT_XX    : 斜牙 (牙型角度倾斜)
 */

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <cstdio>

using namespace cv;

/**
 * 画一个"螺纹工件"
 * @param defect 0=OK 1=缺牙 2=毛刺 3=烂牙 4=斜牙
 */
static void drawThreadPart(Mat& img, Point2f center, double radius,
                           double phaseDeg, int defect, int variant) {
    const int teeth = 36;
    const double step = 360.0 / teeth;

    cv::circle(img, center, int(radius), Scalar(210), -1, cv::LINE_AA);
    cv::circle(img, center, int(radius * 0.22), Scalar(120), -1, cv::LINE_AA);

    for (int i = 0; i < teeth; ++i) {
        double a = phaseDeg + i * step;

        // 缺牙: 固定区间跳过若干牙
        if (defect == 1) {
            const double d0 = std::fmod(phaseDeg + variant * 5.0 + 75.0, 360.0);
            if (std::fmod(a, 360.0) > d0 && std::fmod(a, 360.0) < d0 + step * 4)
                continue;
        }

        // 斜牙: 该区域牙线方向偏转
        double tilt = 0.0;
        if (defect == 4) {
            const double mid = std::fmod(phaseDeg + variant * 7.0 + 120.0, 360.0);
            double rel = std::fmod(a - mid + 360.0, 360.0);
            if (rel < 100.0) tilt = 14.0;
        }
        double a2 = a + tilt;

        const double r1 = radius * 0.80, r2 = radius * 0.97;
        Point2f p1(center.x + float(std::cos(a2 * CV_PI / 180.0) * r1),
                   center.y + float(std::sin(a2 * CV_PI / 180.0) * r1));
        Point2f p2(center.x + float(std::cos(a2 * CV_PI / 180.0) * r2),
                   center.y + float(std::sin(a2 * CV_PI / 180.0) * r2));

        if (defect == 3) {
            // 烂牙: 固定区间牙型画成抖动短弧
            const double mid = std::fmod(phaseDeg + variant * 9.0 + 200.0, 360.0);
            double rel = std::fmod(a - mid + 360.0, 360.0);
            if (rel < 80.0) {
                Point2f jitter(float((i * 7 + variant) % 5) - 2.0f,
                               float((i * 3 + variant) % 5) - 2.0f);
                cv::line(img, p1 + jitter, p2 + Point2f(2, -2), Scalar(90), 5, cv::LINE_AA);
                continue;
            }
        }
        cv::line(img, p1, p2, Scalar(40), 3, cv::LINE_AA);

        // 毛刺: 部分牙顶加亮刺
        if (defect == 2 && i % 3 == variant % 3) {
            Point2f tip(center.x + float(std::cos(a * CV_PI / 180.0) * (radius + 9)),
                        center.y + float(std::sin(a * CV_PI / 180.0) * (radius + 9)));
            cv::line(img, p2, tip, Scalar(255), 2, cv::LINE_AA);
            cv::circle(img, tip, 3, Scalar(255), -1, cv::LINE_AA);
        }
    }
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QStringList args = QCoreApplication::arguments().mid(1);
    QString outDir = args.value(0, QStringLiteral("testdata/virtual_camera"));
    const int okCount = args.value(1, QStringLiteral("24")).toInt();
    const int ngPerType = args.value(2, QStringLiteral("6")).toInt();

    QDir().mkpath(outDir);
    const int W = 640, H = 480;
    const double radius = 110.0;
    int generated = 0;

    // OK样本: 相位均匀分布 + 位置微移
    for (int i = 0; i < okCount; ++i) {
        Mat img(H, W, CV_8UC1, Scalar(48));
        cv::circle(img, Point(W / 2, H / 2), int(std::min(W, H) * 0.48), Scalar(70), 3, cv::LINE_AA);
        double a = 360.0 * i / okCount;
        Point2f c(W * 0.5f + float(std::cos(a * CV_PI / 180.0) * W * 0.12),
                  H * 0.5f + float(std::sin(a * CV_PI / 180.0) * H * 0.12));
        drawThreadPart(img, c, radius, a, 0, 0);
        cv::putText(img, "OK", Point(12, 26), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(200), 1, LINE_AA);
        const QString path = outDir + QString("/OK_%1.png").arg(i + 1, 2, 10, QChar('0'));
        imwrite(path.toLocal8Bit().toStdString(), img);
        ++generated;
    }

    const char* ngNames[] = {"NG_MISSING", "NG_BURR", "NG_DAMAGED", "NG_SLANT"};
    for (int defect = 1; defect <= 4; ++defect) {
        for (int i = 0; i < ngPerType; ++i) {
            Mat img(H, W, CV_8UC1, Scalar(48));
            cv::circle(img, Point(W / 2, H / 2), int(std::min(W, H) * 0.48), Scalar(70), 3, cv::LINE_AA);
            double a = 360.0 * i / std::max(1, ngPerType) + defect * 13.0;
            Point2f c(W * 0.5f + float(std::cos(a * CV_PI / 180.0) * W * 0.12),
                      H * 0.5f + float(std::sin(a * CV_PI / 180.0) * H * 0.12));
            drawThreadPart(img, c, radius, a, defect, i);
            cv::putText(img, ngNames[defect - 1], Point(12, 26),
                        FONT_HERSHEY_SIMPLEX, 0.7, Scalar(200), 1, LINE_AA);
            const QString path = outDir + QString("/%1_%2.png").arg(ngNames[defect - 1]).arg(i + 1, 2, 10, QChar('0'));
            imwrite(path.toLocal8Bit().toStdString(), img);
            ++generated;
        }
    }

    std::printf("已生成 %d 张测试图 -> %s\n", generated, outDir.toLocal8Bit().constData());
    return 0;
}
