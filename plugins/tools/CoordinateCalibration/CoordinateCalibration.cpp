/**
 * @file CoordinateCalibration.cpp
 * @brief 坐标校准工具 — 多点仿射/透视坐标标定 (对标 CKVision 坐标校准1/2)
 *
 * 用途: 通过 ≥3 个(仿射)/≥4 个(透视) 图像坐标点与对应目标坐标点,
 * 用 OpenCV estimateAffine2D / findHomography 拟合坐标变换矩阵,
 * 将任意图像坐标映射到机械/基准坐标, 并输出残差评估标定质量。
 *
 * 属性:
 *   model       仿射(≥3点) / 透视(≥4点)
 *   srcPoints   "x1,y1;x2,y2;..." 图像坐标点集
 *   dstPoints   "x1,y1;x2,y2;..." 目标(机械)坐标点集
 *   queryX/Y    待变换的查询点 (可选, 输出变换后坐标)
 *   applyTo     当前流程 / 所有流程 (写入全局变量)
 *
 * 输出:
 *   matrix 变换矩阵 2x3(仿射)/3x3(透视) [a..i]
 *   rms 平均残差(像素) | maxErr 最大残差 | usedPoints 有效点数
 *   resultX/Y 查询点变换后坐标
 * 上下文: calibration_transform_* (矩阵分量 + rms)
 */
#include "CoordinateCalibration.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/GlobalVariables.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/geometry/3d.hpp>
#endif
#include <cmath>
#include <QStringList>

namespace VisionInspector {

PropertyDefList CoordinateCalibration::propertyDefs() const {
    return {
        PropertyDef::enumProp("model", "变换模型", {"仿射(≥3点)", "透视(≥4点)"}, 0),
        PropertyDef::stringProp("srcPoints", "图像坐标点集", "", "标定点"),
        PropertyDef::stringProp("dstPoints", "目标坐标点集", "", "标定点"),
        PropertyDef::doubleProp("queryX", "查询点X", 0, -1e9, 1e9, "变换点"),
        PropertyDef::doubleProp("queryY", "查询点Y", 0, -1e9, 1e9, "变换点"),
        PropertyDef::enumProp("applyTo", "应用范围", {"当前流程", "所有流程"}, 0),
    };
}

namespace {
// 解析 "x1,y1;x2,y2;..." 点集; 返回 false 表示格式错误
bool parsePoints(const QString& text, std::vector<cv::Point2d>& pts) {
    pts.clear();
    const QStringList pairs = text.split(';', Qt::SkipEmptyParts);
    for (const QString& pair : pairs) {
        const QStringList xy = pair.trimmed().split(',');
        if (xy.size() != 2) return false;
        bool okX = false, okY = false;
        const double x = xy[0].trimmed().toDouble(&okX);
        const double y = xy[1].trimmed().toDouble(&okY);
        if (!okX || !okY) return false;
        pts.push_back({x, y});
    }
    return true;
}
} // namespace

bool CoordinateCalibration::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    const int model = propertyValue("model").toInt();   // 0仿射 1透视
    const QString srcText = propertyValue("srcPoints").toString();
    const QString dstText = propertyValue("dstPoints").toString();

    std::vector<cv::Point2d> src, dst;
    if (!parsePoints(srcText, src) || !parsePoints(dstText, dst)) {
        setResultData("error", "点集格式错误: 应为 x1,y1;x2,y2;...");
        setStatus(ToolStatus::NG);
        return false;
    }
    const size_t n = std::min(src.size(), dst.size());
    if (n < (model == 0 ? 3u : 4u)) {
        setResultData("error",
                      QString("有效点数%1不足(%2需%3点)")
                          .arg(n).arg(model == 0 ? "仿射" : "透视")
                          .arg(model == 0 ? 3 : 4));
        setStatus(ToolStatus::NG);
        return false;
    }
    src.resize(n);
    dst.resize(n);

    cv::Mat M;   // 2x3(仿射) 或 3x3(透视)
    if (model == 0) {
        M = cv::estimateAffine2D(src, dst);   // RANSAC 稳健拟合
        if (M.empty()) {
            setResultData("error", "仿射拟合失败");
            setStatus(ToolStatus::NG);
            return false;
        }
    } else {
        M = cv::findHomography(src, dst);
        if (M.empty()) {
            setResultData("error", "透视拟合失败");
            setStatus(ToolStatus::NG);
            return false;
        }
    }

    // 残差: 每点变换后与目标距离
    double sumErr = 0, maxErr = 0;
    for (size_t i = 0; i < n; ++i) {
        double tx, ty;
        if (model == 0) {
            const double a = M.at<double>(0, 0), b = M.at<double>(0, 1), c = M.at<double>(0, 2);
            const double d = M.at<double>(1, 0), e = M.at<double>(1, 1), f = M.at<double>(1, 2);
            tx = a * src[i].x + b * src[i].y + c;
            ty = d * src[i].x + e * src[i].y + f;
        } else {
            const double x = src[i].x, y = src[i].y;
            const double w = M.at<double>(2, 0) * x + M.at<double>(2, 1) * y + M.at<double>(2, 2);
            const double iw = (w != 0) ? 1.0 / w : 0;
            tx = (M.at<double>(0, 0) * x + M.at<double>(0, 1) * y + M.at<double>(0, 2)) * iw;
            ty = (M.at<double>(1, 0) * x + M.at<double>(1, 1) * y + M.at<double>(1, 2)) * iw;
        }
        const double err = std::hypot(tx - dst[i].x, ty - dst[i].y);
        sumErr += err;
        maxErr = std::max(maxErr, err);
    }
    const double rms = sumErr / n;

    // 查询点变换
    const double qx = propertyValue("queryX").toDouble();
    const double qy = propertyValue("queryY").toDouble();
    double rx = 0, ry = 0;
    if (model == 0) {
        const double a = M.at<double>(0, 0), b = M.at<double>(0, 1), c = M.at<double>(0, 2);
        const double d = M.at<double>(1, 0), e = M.at<double>(1, 1), f = M.at<double>(1, 2);
        rx = a * qx + b * qy + c;
        ry = d * qx + e * qy + f;
    } else {
        const double w = M.at<double>(2, 0) * qx + M.at<double>(2, 1) * qy + M.at<double>(2, 2);
        const double iw = (w != 0) ? 1.0 / w : 0;
        rx = (M.at<double>(0, 0) * qx + M.at<double>(0, 1) * qy + M.at<double>(0, 2)) * iw;
        ry = (M.at<double>(1, 0) * qx + M.at<double>(1, 1) * qy + M.at<double>(1, 2)) * iw;
    }

    // 输出结果
    QVariantList matrixList;
    for (int r = 0; r < M.rows; ++r)
        for (int c = 0; c < M.cols; ++c)
            matrixList.append(M.at<double>(r, c));
    setResultData("matrix", matrixList);
    setResultData("model", model);
    setResultData("rms", rms);
    setResultData("maxErr", maxErr);
    setResultData("usedPoints", (int)n);
    setResultData("resultX", rx);
    setResultData("resultY", ry);
    setResultData("calibrated", true);

    // 写入上下文 (供下游做坐标映射)
    context.setData("calibration_transform_model", model);
    context.setData("calibration_transform_rms", rms);
    context.setData("calibration_transform_used", (int)n);
    for (int i = 0; i < matrixList.size(); ++i)
        context.setData(QString("calibration_transform_%1").arg(i), matrixList[i].toDouble());

    if (propertyValue("applyTo").toInt() == 1) {
        if (auto* gv = context.globalVariables()) {
            gv->set("calibration_transform_model", model);
            gv->set("calibration_transform_rms", rms);
            for (int i = 0; i < matrixList.size(); ++i)
                gv->set(QString("calibration_transform_%1").arg(i), matrixList[i].toDouble());
        }
    }

    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库");
    setStatus(ToolStatus::NG);
    return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(CoordinateCalibration, "坐标校准", VisionInspector::ToolCategory::Calibration)
