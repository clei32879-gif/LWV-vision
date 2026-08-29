/**
 * @file Common.h
 * @brief 通用定义和类型
 *
 * 如果OpenCV不可用, 用替代类型, 保证项目能编译。
 * 后续链接OpenCV后, 恢复使用cv::Mat。
 */

#pragma once

#include <QString>
#include <QVariant>
#include <QImage>
#include <cstdint>
#include <vector>
#include <memory>

// 尝试包含OpenCV (如果编译时找不到, 用替代类型)
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

// ============================================================
// 版本信息
// ============================================================
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

inline QString versionString() {
    return QString("%1.%2.%3")
        .arg(VERSION_MAJOR)
        .arg(VERSION_MINOR)
        .arg(VERSION_PATCH);
}

// ============================================================
// 用户权限等级
// ============================================================
enum class UserRole {
    Operator = 0,
    Technician = 1,
    Admin = 2
};

inline bool operator>=(UserRole a, UserRole b) {
    return static_cast<int>(a) >= static_cast<int>(b);
}

inline QString roleToString(UserRole role) {
    switch (role) {
        case UserRole::Operator:   return QStringLiteral("操作员");
        case UserRole::Technician: return QStringLiteral("技术员");
        case UserRole::Admin:      return QStringLiteral("管理员");
    }
    return QStringLiteral("未知");
}

// ============================================================
// 工具执行状态
// ============================================================
enum class ToolStatus {
    Idle,
    Running,
    OK,
    NG,
    Error,
    Disabled
};

inline QString statusToString(ToolStatus status) {
    switch (status) {
        case ToolStatus::Idle:     return QStringLiteral("待机");
        case ToolStatus::Running:  return QStringLiteral("运行中");
        case ToolStatus::OK:       return QStringLiteral("OK");
        case ToolStatus::NG:       return QStringLiteral("NG");
        case ToolStatus::Error:    return QStringLiteral("错误");
        case ToolStatus::Disabled: return QStringLiteral("禁用");
    }
    return QStringLiteral("未知");
}

// ============================================================
// 工具分类
// ============================================================
enum class ToolCategory {
    Camera,
    ImageProcess,
    Calibration,
    Detection,
    Geometry,
    Communication,
    Logic,
    System,
    ThreeD,
    Special
};

inline QString categoryToString(ToolCategory cat) {
    switch (cat) {
        case ToolCategory::Camera:        return QStringLiteral("相机工具");
        case ToolCategory::ImageProcess:  return QStringLiteral("图像处理");
        case ToolCategory::Calibration:   return QStringLiteral("标定校准");
        case ToolCategory::Detection:     return QStringLiteral("检测识别");
        case ToolCategory::Geometry:      return QStringLiteral("几何测量");
        case ToolCategory::Communication: return QStringLiteral("文件通讯");
        case ToolCategory::Logic:         return QStringLiteral("逻辑控制");
        case ToolCategory::System:        return QStringLiteral("系统工具");
        case ToolCategory::ThreeD:        return QStringLiteral("三维测量");
        case ToolCategory::Special:       return QStringLiteral("专用工具");
    }
    return QStringLiteral("其他");
}

// ============================================================
// 图像类型
// ============================================================
#ifdef VI_HAS_OPENCV
// 有OpenCV时, 使用cv::Mat
using CvImage = cv::Mat;
using CvImagePtr = std::shared_ptr<cv::Mat>;
#else
// 无OpenCV时, 用QImage作为替代, 保证能编译
using CvImage = QImage;
using CvImagePtr = std::shared_ptr<QImage>;
#endif

// QVariant映射
using DataMap = QMap<QString, QVariant>;

// ============================================================
// 图像转换
// ============================================================
#ifdef VI_HAS_OPENCV
inline cv::Mat qImageToCvMat(const QImage& img) {
    cv::Mat mat;
    switch (img.format()) {
        case QImage::Format_RGB888: {
            mat = cv::Mat(img.height(), img.width(), CV_8UC3,
                          const_cast<uchar*>(img.bits()), img.bytesPerLine()).clone();
            cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
            break;
        }
        case QImage::Format_Grayscale8: {
            mat = cv::Mat(img.height(), img.width(), CV_8UC1,
                          const_cast<uchar*>(img.bits()), img.bytesPerLine()).clone();
            break;
        }
        default: {
            QImage converted = img.convertToFormat(QImage::Format_RGB888);
            mat = cv::Mat(converted.height(), converted.width(), CV_8UC3,
                          const_cast<uchar*>(converted.bits()), converted.bytesPerLine()).clone();
            cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
            break;
        }
    }
    return mat;
}

inline QImage cvMatToQImage(const cv::Mat& mat) {
    if (mat.empty()) return QImage();
    switch (mat.type()) {
        case CV_8UC1: {
            QImage img(mat.data, mat.cols, mat.rows,
                       static_cast<int>(mat.step), QImage::Format_Grayscale8);
            return img.copy();
        }
        case CV_8UC3: {
            cv::Mat rgb;
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
            QImage img(rgb.data, rgb.cols, rgb.rows,
                       static_cast<int>(rgb.step), QImage::Format_RGB888);
            return img.copy();
        }
        case CV_8UC4: {
            QImage img(mat.data, mat.cols, mat.rows,
                       static_cast<int>(mat.step), QImage::Format_RGBA8888);
            return img.copy();
        }
        default:
            return QImage();
    }
}
#else
// 无OpenCV时的简单实现
inline QImage qImageToCvMat(const QImage& img) {
    return img.copy();
}

inline QImage cvMatToQImage(const CvImage& mat) {
    return mat.copy();
}
#endif

} // namespace VisionInspector
