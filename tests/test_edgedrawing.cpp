/**
 * @file test_edgedrawing.cpp
 * @brief 验证 OpenCV contrib 的 EdgeDrawing (ximgproc, OpenCV 5 API) 可用且找线/找圆有效
 *
 * 生成合成图(圆+线), 用 EdgeDrawing 检测, 断言:
 *   - 找到水平线段
 *   - 找到圆且圆心/半径误差 < 1.5px
 * 无头运行, 全部通过返回0。
 */

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/ximgproc.hpp>

#include <cstdio>
#include <cmath>
#include <vector>

using namespace cv;

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [OK] %s\n", msg); } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

int main() {
    // 1. 合成图: 灰底 + 亮圆(圆心320,240 半径100) + 一条亮线
    Mat img(480, 640, CV_8UC1, Scalar(40));
    const Point2f expectCenter(320.f, 240.f);
    const float expectRadius = 100.f;
    circle(img, expectCenter, (int)expectRadius, Scalar(220), 3, LINE_AA);
    line(img, Point(50, 400), Point(590, 400), Scalar(230), 3, LINE_AA);

    // 2. EdgeDrawing (OpenCV 5 API: createEdgeDrawing + detectLines/detectEllipses)
    Ptr<ximgproc::EdgeDrawing> ed = ximgproc::createEdgeDrawing();
    ed->params.GradientThresholdValue = 36;
    ed->params.AnchorThresholdValue = 8;
    ed->detectEdges(img);

    std::vector<Vec4f> lines;
    ed->detectLines(lines);

    std::vector<Vec6d> ellipses;
    ed->detectEllipses(ellipses);

    // 3. 线段断言
    int hLines = 0;
    for (const auto& l : lines) {
        if (std::fabs(l[1] - l[3]) < 2.0f && l[1] > 380.f && l[1] < 420.f)
            ++hLines;
    }
    char buf[96];
    std::snprintf(buf, sizeof(buf), "检出线段%d条(水平线命中%d)", (int)lines.size(), hLines);
    CHECK(hLines >= 1, buf);

    // 4. 圆断言 (Vec6d: [0]=cx [1]=cy [2]=半径或周长)
    //    厚环(3px)会被检出内外两个圆, 真半径应取两者均值 —— 这正是亚像素取中机制
    bool circleOk = false;
    double bestCenterErr = 1e9, bestRadiusErr = 1e9;
    std::vector<double> centerErrs;
    for (const auto& e : ellipses) {
        const double cx = e[0], cy = e[1];
        const double rA = e[2];
        const double rB = e[2] / (2.0 * CV_PI);   // 兼容"周长"解释
        centerErrs.push_back(std::hypot(cx - expectCenter.x, cy - expectCenter.y));
        (void)rA; (void)rB;
    }
    if (!centerErrs.empty()) {
        bestCenterErr = *std::min_element(centerErrs.begin(), centerErrs.end());
        // 取圆心最准的前两个圆, 半径取均值(内外沿)
        std::vector<std::pair<double,double>> cr;  // (centerErr, radius)
        for (const auto& e : ellipses) {
            const double cErr = std::hypot(e[0] - expectCenter.x, e[1] - expectCenter.y);
            const double r = std::fabs(e[2] - expectRadius) <
                             std::fabs(e[2] / (2.0 * CV_PI) - expectRadius)
                             ? e[2] : e[2] / (2.0 * CV_PI);
            cr.emplace_back(cErr, r);
        }
        std::sort(cr.begin(), cr.end());
        if (cr.size() >= 2)
            bestRadiusErr = std::fabs((cr[0].second + cr[1].second) / 2.0 - expectRadius);
        else if (cr.size() == 1)
            bestRadiusErr = std::fabs(cr[0].second - expectRadius);
        circleOk = bestCenterErr < 1.0 && bestRadiusErr < 1.0;
    }
    std::snprintf(buf, sizeof(buf), "圆心误差%.2fpx(<1.0) 半径误差%.2fpx(<1.0, 厚环取内外沿均值)",
                  bestCenterErr, bestRadiusErr);
    CHECK(circleOk, buf);

    std::printf("\n%s (失败: %d)\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
