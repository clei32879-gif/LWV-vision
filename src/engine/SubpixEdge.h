/**
 * @file SubpixEdge.h
 * @brief 亚像素边缘测量工具库 (阶段2算法升级)
 *
 * 提供:
 *   - 双线性灰度采样
 *   - 卡尺式亚像素边缘搜索 (梯度峰值 + 抛物线插值)
 *   - 鲁棒圆/直线拟合 (代数拟合 + 中位数残差外点剔除)
 *
 * 被 检测圆形/检测直线/卡尺 等测量类工具共享使用。
 */

#pragma once
#include "../utils/Common.h"
#include <vector>

#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>

namespace VisionInspector {

// ============================================================
// 参数与结果
// ============================================================

struct ScanOptions {
    int polarity = 0;            // 0=任意 1=亮到暗(负梯度) 2=暗到亮(正梯度)
    double gradThreshold = 40;   // 梯度幅值阈值
    int filterHalfWidth = 2;     // 剖面平滑半宽(移动平均)
};

struct SubpixEdgePoint {
    cv::Point2d pos;             // 亚像素位置
    double strength = 0;         // 梯度幅值
    double signedGrad = 0;       // 带符号梯度 (亮到暗为负)
};

// ============================================================
// 接口
// ============================================================

/** 双线性灰度采样 (越界取边界值) */
double sampleBilinear(const cv::Mat& gray, double x, double y);

/**
 * 卡尺式亚像素边缘搜索: 沿线段 p0→p1 采样剖面, 找最强边缘并亚像素内插
 * @param out 输出边缘点与强度
 * @return false=该方向无超阈值边缘
 */
bool findEdgeSubpix(const cv::Mat& gray, const cv::Point2d& p0, const cv::Point2d& p1,
                    const ScanOptions& opt, SubpixEdgePoint& out);

/**
 * 多边缘版本: 返回剖面上所有超阈值的局部梯度峰值(按扫描顺序)
 * @param maxCount 0=全部, 否则只取最强的前maxCount个
 */
std::vector<SubpixEdgePoint> findEdgesSubpix(const cv::Mat& gray,
                                             const cv::Point2d& p0, const cv::Point2d& p1,
                                             const ScanOptions& opt, int maxCount = 0);

/**
 * 鲁棒圆拟合: Kasa代数拟合 + 中位数残差外点剔除(最多2轮)
 * @param rms 保留点的均方根残差
 * @return false=点数不足(<3)或退化
 */
bool fitCircleRobust(const std::vector<cv::Point2d>& pts,
                     cv::Point2d& center, double& radius, double& rms);

/**
 * 鲁棒直线拟合: 质心+PCA主方向 + 残差外点剔除(最多2轮)
 * @param dir 单位方向向量, origin 直线上一点(质心), rms 均方根残差
 */
bool fitLineRobust(const std::vector<cv::Point2d>& pts,
                   cv::Point2d& dir, cv::Point2d& origin, double& rms);

} // namespace VisionInspector
#endif // VI_HAS_OPENCV
