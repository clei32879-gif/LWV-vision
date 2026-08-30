/**
 * @file Calibration.cpp
 * @brief 标定校准工具（像素→毫米转换 + 真棋盘格标定）
 *
 * 三种标定方式:
 *   0 两点标定: 手动输入像素长度与实际长度换算
 *   1 已知比例: 直接输入 mm/pixel
 *   2 棋盘格标定: 对输入图像做 findChessboardCorners 角点检测,
 *     由相邻角点间距(像素)与实际格子尺寸自动计算 mm/pixel (标定真实化 P0-3)
 *
 * 标定结果统一写入上下文: calibration_ratio(px→mm), calibration_pixel_per_mm,
 * calibration_mm_per_pixel 等, 供后续测量/检测工具换算使用。
 */
#include "Calibration.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/calib.hpp>
#endif
#include <algorithm>
#include <cmath>

namespace VisionInspector {

PropertyDefList Calibration::propertyDefs() const {
    return {
        PropertyDef::enumProp("calibMethod", "标定方法", {"两点标定", "已知比例", "棋盘格标定"}, 1),
        // 两点标定
        PropertyDef::doubleProp("pixelLength", "像素长度", 100.0, 1, 10000),
        PropertyDef::doubleProp("realLength", "实际长度(mm)", 10.0, 0.001, 10000),
        // 已知比例
        PropertyDef::doubleProp("pixelRatio", "像素比例(mm/pixel)", 0.1, 0.0001, 100),
        // 棋盘格标定参数
        PropertyDef::intProp("boardCols", "内角点列数", 9, 3, 30),
        PropertyDef::intProp("boardRows", "内角点行数", 6, 3, 30),
        PropertyDef::doubleProp("squareSize", "格子边长(mm)", 10.0, 0.1, 1000),
        PropertyDef::stringProp("unit", "单位", "mm"),
        PropertyDef::enumProp("applyTo", "应用范围", {"当前流程", "所有流程"}, 0),
    };
}

bool Calibration::execute(ToolContext& context) {
    int method = propertyValue("calibMethod").toInt();
    double ratio = 0;

    if (method == 0) {
        // 两点标定
        double pixelLen = propertyValue("pixelLength").toDouble();
        double realLen = propertyValue("realLength").toDouble();
        if (pixelLen > 0) {
            ratio = realLen / pixelLen;
        }
    } else if (method == 1) {
        // 已知比例
        ratio = propertyValue("pixelRatio").toDouble();
    } else {
        // 棋盘格标定 (真角点检测, 自动计算比例)
#ifdef VI_HAS_OPENCV
        CvImagePtr input = getInputImage(context);
        if (!input || input->empty()) {
            setResultData("error", "输入图像为空");
            setStatus(ToolStatus::NG);
            return false;
        }
        const int cols = propertyValue("boardCols").toInt();
        const int rows = propertyValue("boardRows").toInt();
        const double squareSize = propertyValue("squareSize").toDouble();
        if (cols < 2 || rows < 2 || squareSize <= 0) {
            setResultData("error", "棋盘格参数无效");
            setStatus(ToolStatus::NG);
            return false;
        }

        cv::Mat gray;
        if (input->channels() > 1) cv::cvtColor(*input, gray, cv::COLOR_BGR2GRAY);
        else gray = *input;

        // 角点检测 (solving-board 对噪声/光照更鲁棒)
        std::vector<cv::Point2f> corners;
        const cv::Size pattern(cols, rows);
        const bool ok = cv::findChessboardCornersSB(gray, pattern, corners);
        setResultData("cornersFound", ok);
        if (!ok || corners.empty()) {
            setResultData("error", "未检测到棋盘格角点");
            setStatus(ToolStatus::NG);
            return false;
        }

        // 亚像素精化
        std::vector<cv::Point2f> refined = corners;
        cv::cornerSubPix(gray, refined, cv::Size(5, 5), cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01));

        // 用相邻角点间距估计"单格平均像素边长":
        //   水平相邻: 每行内相邻 (cols-1)*rows 对
        //   垂直相邻: 每列内相邻 cols*(rows-1) 对
        std::vector<double> gaps;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols - 1; ++c) {
                const int i = r * cols + c;
                gaps.push_back(std::hypot(refined[i+1].x - refined[i].x,
                                          refined[i+1].y - refined[i].y));
            }
        }
        for (int c = 0; c < cols; ++c) {
            for (int r = 0; r < rows - 1; ++r) {
                const int i = r * cols + c;
                gaps.push_back(std::hypot(refined[i+cols].x - refined[i].x,
                                          refined[i+cols].y - refined[i].y));
            }
        }
        if (gaps.empty()) {
            setResultData("error", "角点间距计算失败");
            setStatus(ToolStatus::NG);
            return false;
        }
        // 中位数(抗外点)
        std::sort(gaps.begin(), gaps.end());
        const double avgPx = gaps[gaps.size() / 2];
        if (avgPx <= 0) {
            setResultData("error", "角点间距为0");
            setStatus(ToolStatus::NG);
            return false;
        }
        ratio = squareSize / avgPx;   // mm/pixel

        // 输出角点网格(供其它工具/展示用), 按行组织的 [x,y] 列表
        QVariantList grid;
        for (const auto& p : refined)
            grid.append(QVariantList{p.x, p.y});
        setResultData("cornerGrid", grid);
        setResultData("cornerCount", (int)refined.size());
        setResultData("avgPixelPerSquare", avgPx);
#else
        setResultData("error", "需要OpenCV库");
        setStatus(ToolStatus::NG);
        return false;
#endif
    }

    if (ratio <= 0) {
        setResultData("error", "标定比例无效");
        setStatus(ToolStatus::NG);
        return false;
    }

    QString unit = propertyValue("unit").toString();

    // 将标定信息存入上下文，供后续工具使用
    context.setData("calibration_ratio", ratio);
    context.setData("calibration_unit", unit);
    context.setData("calibration_mm_per_pixel", ratio);
    context.setData("calibration_pixel_per_mm", 1.0 / ratio);

    setResultData("mmPerPixel", ratio);
    setResultData("pixelPerMm", 1.0 / ratio);
    setResultData("unit", unit);
    setResultData("calibrated", true);

    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(Calibration, "标定校准", VisionInspector::ToolCategory::Calibration)
