/**
 * @file ImageMerge.cpp
 * @brief 图像合并工具 (对标 CKVision 图像合并, P1-11 补齐)
 *
 * 把多张图像 (inputImage + inputImage2/3/4, 最多4张) 合并成一张大图:
 * - 方向: 水平拼接 / 垂直拼接 / 网格(行x列)
 * - 水平/垂直重叠: 重叠区取两张图像平均值混合 (去接缝)
 * - 尺寸统一: 以第一张图为准, 其余自动缩放对齐
 *
 * 输出: 合并结果写入 Current + 尺寸
 */
#include "ImageMerge.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif
#include <vector>

namespace VisionInspector {

PropertyDefList ImageMerge::propertyDefs() const {
    return {
        PropertyDef::enumProp("direction", "合并方向", {"水平拼接", "垂直拼接", "网格(行x列)"}, 0),
        PropertyDef::stringProp("inputImage2", "输入图像2", ""),
        PropertyDef::stringProp("inputImage3", "输入图像3", ""),
        PropertyDef::stringProp("inputImage4", "输入图像4", ""),
        PropertyDef::intProp("rows", "网格行数", 1, 1, 8),
        PropertyDef::intProp("cols", "网格列数", 2, 1, 8),
        PropertyDef::intProp("horizontalOverlap", "水平重叠", 0, 0, 1000),
        PropertyDef::intProp("verticalOverlap", "垂直重叠", 0, 0, 1000),
    };
}

namespace {
/** 读取命名图像槽, 空名返回 nullptr */
CvImagePtr readSlot(ToolContext& ctx, const QString& name) {
    if (name.isEmpty() || name == "Current") return nullptr;
    return ctx.getImage(name);
}
/** 每张统一为同一尺寸 (以第一张为准) */
void alignSizes(std::vector<cv::Mat>& imgs, cv::Size ref) {
    for (auto& m : imgs) {
        if (m.size() != ref) cv::resize(m, m, ref, 0, 0, cv::INTER_LINEAR);
    }
}
/** 水平拼接带重叠 (重叠区平均) */
cv::Mat hMerge(const std::vector<cv::Mat>& imgs, int overlap) {
    const int n = (int)imgs.size();
    if (n == 1) return imgs[0].clone();
    const int rows = imgs[0].rows;
    int totalW = 0;
    for (const auto& m : imgs) totalW += m.cols;
    if (overlap > 0) totalW -= overlap * (n - 1);
    if (totalW < 1) totalW = 1;
    cv::Mat out(rows, totalW, imgs[0].type(), cv::Scalar::all(0));
    int x = 0;
    for (int i = 0; i < n; ++i) {
        const cv::Mat& m = imgs[i];
        if (i == 0) {
            m.copyTo(out(cv::Rect(0, 0, m.cols, rows)));
            x = m.cols;
        } else {
            const int ov = std::min(overlap, m.cols);
            const int start = x - ov;            // 重叠区起点 (含)
            const int olen = ov;                 // 重叠宽度
            if (olen > 0) {
                // 重叠区: 前图残段与新图平均混合
                for (int j = 0; j < olen; ++j) {
                    const double a = (double)(olen - j) / olen;      // 前图权重
                    const double b = 1.0 - a;
                    cv::Mat roi = out(cv::Rect(start + j, 0, 1, rows));
                    cv::Mat cur = m(cv::Rect(j, 0, 1, rows));
                    roi = roi * a + cur * b;
                }
            }
            m(cv::Rect(ov, 0, m.cols - ov, rows)).copyTo(out(cv::Rect(x, 0, m.cols - ov, rows)));
            x += m.cols - ov;
        }
    }
    return out;
}
/** 垂直拼接带重叠 (重叠区平均) */
cv::Mat vMerge(const std::vector<cv::Mat>& imgs, int overlap) {
    const int n = (int)imgs.size();
    if (n == 1) return imgs[0].clone();
    const int cols = imgs[0].cols;
    int totalH = 0;
    for (const auto& m : imgs) totalH += m.rows;
    if (overlap > 0) totalH -= overlap * (n - 1);
    if (totalH < 1) totalH = 1;
    cv::Mat out(totalH, cols, imgs[0].type(), cv::Scalar::all(0));
    int y = 0;
    for (int i = 0; i < n; ++i) {
        const cv::Mat& m = imgs[i];
        if (i == 0) {
            m.copyTo(out(cv::Rect(0, 0, cols, m.rows)));
            y = m.rows;
        } else {
            const int ov = std::min(overlap, m.rows);
            const int start = y - ov;
            const int olen = ov;
            if (olen > 0) {
                for (int j = 0; j < olen; ++j) {
                    const double a = (double)(olen - j) / olen;
                    const double b = 1.0 - a;
                    cv::Mat roi = out(cv::Rect(0, start + j, cols, 1));
                    cv::Mat cur = m(cv::Rect(0, j, cols, 1));
                    roi = roi * a + cur * b;
                }
            }
            m(cv::Rect(0, ov, cols, m.rows - ov)).copyTo(out(cv::Rect(0, y, cols, m.rows - ov)));
            y += m.rows - ov;
        }
    }
    return out;
}
} // namespace

bool ImageMerge::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr first = getInputImage(context);
    if (!first || first->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    // 收集输入 (含命名槽 2/3/4)
    std::vector<cv::Mat> imgs;
    cv::Mat base = *first;
    if (base.channels() == 1) cv::cvtColor(base, base, cv::COLOR_GRAY2BGR);   // 统一彩色便于混合
    imgs.push_back(base.clone());
    const QString names[3] = {
        propertyValue("inputImage2").toString(),
        propertyValue("inputImage3").toString(),
        propertyValue("inputImage4").toString()
    };
    for (const QString& nm : names) {
        CvImagePtr slot = readSlot(context, nm);
        if (!slot || slot->empty()) continue;
        cv::Mat m = *slot;
        if (m.channels() == 1) cv::cvtColor(m, m, cv::COLOR_GRAY2BGR);
        imgs.push_back(m.clone());
        if ((int)imgs.size() >= 4) break;
    }

    const int dir = propertyValue("direction").toInt();
    const int hOv = propertyValue("horizontalOverlap").toInt();
    const int vOv = propertyValue("verticalOverlap").toInt();
    cv::Mat out;

    if (dir == 0) {          // 水平拼接
        alignSizes(imgs, cv::Size(imgs[0].cols, imgs[0].rows));
        out = hMerge(imgs, hOv);
    } else if (dir == 1) {   // 垂直拼接
        alignSizes(imgs, cv::Size(imgs[0].cols, imgs[0].rows));
        out = vMerge(imgs, vOv);
    } else {                 // 网格
        const int rows = std::max(1, propertyValue("rows").toInt());
        const int cols = std::max(1, propertyValue("cols").toInt());
        alignSizes(imgs, cv::Size(imgs[0].cols, imgs[0].rows));
        const int cellW = imgs[0].cols, cellH = imgs[0].rows;
        out = cv::Mat::zeros(rows * cellH - std::max(0, vOv) * (rows - 1),
                             cols * cellW - std::max(0, hOv) * (cols - 1),
                             imgs[0].type());
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const int idx = r * cols + c;
                if (idx >= (int)imgs.size()) break;
                const int x = c * (cellW - hOv);
                const int y = r * (cellH - vOv);
                imgs[idx].copyTo(out(cv::Rect(x, y, cellW, cellH)));
            }
        }
    }

    setOutputImage(context, std::make_shared<CvImage>(out.clone()));
    setResultData("outputWidth", out.cols);
    setResultData("outputHeight", out.rows);
    setResultData("mergedCount", (int)imgs.size());
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(ImageMerge, "图像合并", VisionInspector::ToolCategory::ImageProcess)
