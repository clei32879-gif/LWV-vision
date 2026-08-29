/**
 * @file SubpixEdge.cpp
 * @brief 亚像素边缘测量工具库实现
 */

#include "SubpixEdge.h"

#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace VisionInspector {

double sampleBilinear(const cv::Mat& gray, double x, double y) {
    if (gray.empty()) return 0;
    const int maxX = gray.cols - 1, maxY = gray.rows - 1;
    x = std::clamp(x, 0.0, (double)maxX);
    y = std::clamp(y, 0.0, (double)maxY);
    const int x0 = (int)x, y0 = (int)y;
    const int x1 = std::min(x0 + 1, maxX), y1 = std::min(y0 + 1, maxY);
    const double fx = x - x0, fy = y - y0;
    const double v00 = gray.at<uchar>(y0, x0), v10 = gray.at<uchar>(y0, x1);
    const double v01 = gray.at<uchar>(y1, x0), v11 = gray.at<uchar>(y1, x1);
    return v00 * (1 - fx) * (1 - fy) + v10 * fx * (1 - fy)
         + v01 * (1 - fx) * fy       + v11 * fx * fy;
}

namespace {

/** 剖面平滑(移动平均) + 中心差分梯度 */
std::vector<double> gradientProfile(const std::vector<double>& s, int halfW) {
    const int n = (int)s.size();
    std::vector<double> sm(n, 0.0);
    if (halfW <= 0) {
        sm = s;
    } else {
        for (int i = 0; i < n; ++i) {
            double sum = 0; int cnt = 0;
            for (int k = -halfW; k <= halfW; ++k) {
                const int j = std::clamp(i + k, 0, n - 1);
                sum += s[j]; ++cnt;
            }
            sm[i] = sum / cnt;
        }
    }
    std::vector<double> g(n, 0.0);
    for (int i = 1; i < n - 1; ++i)
        g[i] = (sm[i + 1] - sm[i - 1]) * 0.5;
    return g;
}

/** 抛物线三点插值求峰的亚像素偏移 [-0.5, 0.5] */
double parabolicOffset(double gm, double g0, double gp) {
    const double denom = gm - 2.0 * g0 + gp;
    if (std::fabs(denom) < 1e-12) return 0;
    return std::clamp(0.5 * (gm - gp) / denom, -0.5, 0.5);
}

} // anonymous namespace

std::vector<SubpixEdgePoint> findEdgesSubpix(const cv::Mat& gray,
                                             const cv::Point2d& p0, const cv::Point2d& p1,
                                             const ScanOptions& opt, int maxCount) {
    std::vector<SubpixEdgePoint> out;
    if (gray.empty()) return out;

    const double len = std::hypot(p1.x - p0.x, p1.y - p0.y);
    const int steps = std::max(4, (int)std::ceil(len));
    std::vector<double> profile(steps + 1);
    for (int i = 0; i <= steps; ++i) {
        const double t = (double)i / steps;
        profile[i] = sampleBilinear(gray, p0.x + (p1.x - p0.x) * t,
                                    p0.y + (p1.y - p0.y) * t);
    }
    const std::vector<double> g = gradientProfile(profile, opt.filterHalfWidth);

    // 收集超阈值的局部峰值
    std::vector<int> peaks;
    for (int i = 1; i < steps; ++i) {
        const double a = g[i - 1], b = g[i], c = g[i + 1];
        const bool isPeak = std::fabs(b) >= std::fabs(a) && std::fabs(b) >= std::fabs(c);
        if (!isPeak) continue;
        bool match = false;
        switch (opt.polarity) {
            case 1: match = (b <= -opt.gradThreshold); break;   // 亮到暗
            case 2: match = (b >=  opt.gradThreshold); break;   // 暗到亮
            default: match = std::fabs(b) >= opt.gradThreshold; break;
        }
        if (match) peaks.push_back(i);
    }
    if (peaks.empty()) return out;

    // 只保留幅值最大的连续峰(相邻峰合并)
    std::sort(peaks.begin(), peaks.end(), [&g](int a, int b) {
        return std::fabs(g[a]) > std::fabs(g[b]);
    });

    for (int idx : peaks) {
        if (maxCount > 0 && (int)out.size() >= maxCount) break;
        SubpixEdgePoint p;
        const double off = parabolicOffset(g[idx - 1], g[idx], g[idx + 1]);
        const double t = ((double)idx + off) / steps;
        p.pos = cv::Point2d(p0.x + (p1.x - p0.x) * t,
                            p0.y + (p1.y - p0.y) * t);
        p.signedGrad = g[idx];
        p.strength = std::fabs(g[idx]);
        out.push_back(p);
    }
    return out;
}

bool findEdgeSubpix(const cv::Mat& gray, const cv::Point2d& p0, const cv::Point2d& p1,
                    const ScanOptions& opt, SubpixEdgePoint& out) {
    auto pts = findEdgesSubpix(gray, p0, p1, opt, 1);
    if (pts.empty()) return false;
    out = pts.front();
    return true;
}

// ============================================================
// 鲁棒圆拟合
// ============================================================
namespace {

/** Kasa代数圆拟合 (单次) */
bool fitCircleOnce(const std::vector<cv::Point2d>& pts,
                   cv::Point2d& center, double& radius) {
    const int n = (int)pts.size();
    if (n < 3) return false;
    double sx = 0, sy = 0, sx2 = 0, sy2 = 0, sxy = 0,
           sx3 = 0, sy3 = 0, sx2y = 0, sxy2 = 0;
    for (const auto& p : pts) {
        const double x = p.x, y = p.y;
        sx += x; sy += y; sx2 += x * x; sy2 += y * y; sxy += x * y;
        sx3 += x * x * x; sy3 += y * y * y; sx2y += x * x * y; sxy2 += x * y * y;
    }
    const double A = n * sx2 - sx * sx;
    const double B = n * sxy - sx * sy;
    const double C = n * sy2 - sy * sy;
    const double D = 0.5 * (n * sx3 + n * sxy2 - sx * sx2 - sx * sy2);
    const double E = 0.5 * (n * sx2y + n * sy3 - sy * sx2 - sy * sy2);
    const double denom = A * C - B * B;
    if (std::fabs(denom) < 1e-9) return false;
    center.x = (D * C - B * E) / denom;
    center.y = (A * E - B * D) / denom;
    double r = 0;
    for (const auto& p : pts)
        r += std::hypot(p.x - center.x, p.y - center.y);
    radius = r / n;
    return true;
}

} // anonymous namespace

bool fitCircleRobust(const std::vector<cv::Point2d>& ptsIn,
                     cv::Point2d& center, double& radius, double& rms) {
    std::vector<cv::Point2d> pts = ptsIn;
    for (int round = 0; round < 3 && (int)pts.size() >= 3; ++round) {
        if (!fitCircleOnce(pts, center, radius)) return false;
        // 残差 → 中位数, 剔除离群点
        std::vector<double> res(pts.size());
        for (size_t i = 0; i < pts.size(); ++i)
            res[i] = std::fabs(std::hypot(pts[i].x - center.x, pts[i].y - center.y) - radius);
        std::vector<double> sorted = res;
        std::sort(sorted.begin(), sorted.end());
        const double med = sorted[sorted.size() / 2];
        const double cut = std::max(3.0 * med, 0.35);
        std::vector<cv::Point2d> keep;
        for (size_t i = 0; i < pts.size(); ++i)
            if (res[i] <= cut) keep.push_back(pts[i]);
        if ((int)keep.size() == (int)pts.size()) break;   // 无外点
        if ((int)keep.size() < 3) break;
        pts = keep;
    }
    if (!fitCircleOnce(pts, center, radius)) return false;
    double sse = 0;
    for (const auto& p : pts) {
        const double d = std::hypot(p.x - center.x, p.y - center.y) - radius;
        sse += d * d;
    }
    rms = std::sqrt(sse / pts.size());
    return true;
}

// ============================================================
// 鲁棒直线拟合
// ============================================================

bool fitLineRobust(const std::vector<cv::Point2d>& ptsIn,
                   cv::Point2d& dir, cv::Point2d& origin, double& rms) {
    std::vector<cv::Point2d> pts = ptsIn;
    if ((int)pts.size() < 2) return false;

    for (int round = 0; round < 3; ++round) {
        // 质心
        cv::Point2d c(0, 0);
        for (const auto& p : pts) { c.x += p.x; c.y += p.y; }
        c.x /= pts.size(); c.y /= pts.size();
        // 协方差主方向 (2x2对称矩阵 [a b; b d] 的最大特征值对应特征向量 (b, λ-a))
        double a = 0, b = 0, d = 0;
        for (const auto& p : pts) {
            const double dx = p.x - c.x, dy = p.y - c.y;
            a += dx * dx; b += dx * dy; d += dy * dy;
        }
        if (std::fabs(b) < 1e-9) {
            // 轴对齐退化: 主方差在x或y
            dir = (a >= d) ? cv::Point2d(1, 0) : cv::Point2d(0, 1);
        } else {
            const double tr = a + d;
            const double det = a * d - b * b;
            const double lam = tr * 0.5 + std::sqrt(std::max(0.0, tr * tr * 0.25 - det));
            cv::Point2d dirV(b, lam - a);
            const double norm = std::hypot(dirV.x, dirV.y);
            dir = cv::Point2d(dirV.x / norm, dirV.y / norm);
        }
        origin = c;

        if (round == 2) break;
        // 残差(点到直线距离) → 剔除离群
        std::vector<double> res(pts.size());
        for (size_t i = 0; i < pts.size(); ++i) {
            const double dx = pts[i].x - c.x, dy = pts[i].y - c.y;
            res[i] = std::fabs(dx * dir.y - dy * dir.x);
        }
        std::vector<double> sorted = res;
        std::sort(sorted.begin(), sorted.end());
        const double med = sorted[sorted.size() / 2];
        const double cut = std::max(3.0 * med, 0.35);
        std::vector<cv::Point2d> keep;
        for (size_t i = 0; i < pts.size(); ++i)
            if (res[i] <= cut) keep.push_back(pts[i]);
        if ((int)keep.size() == (int)pts.size() || (int)keep.size() < 2) break;
        pts = keep;
    }

    double sse = 0;
    for (const auto& p : pts) {
        const double dx = p.x - origin.x, dy = p.y - origin.y;
        const double r = dx * dir.y - dy * dir.x;
        sse += r * r;
    }
    rms = std::sqrt(sse / pts.size());
    return true;
}

} // namespace VisionInspector
#endif // VI_HAS_OPENCV
