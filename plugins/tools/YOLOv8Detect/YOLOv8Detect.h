/**
 * @file YOLOv8Detect.h
 * @brief YOLOv8 目标检测与实例分割工具
 *
 * 基于 ONNX Runtime 推理引擎，加载 YOLOv8 ONNX 模型进行推理。
 * 支持：
 *   - 目标检测 (detect)：输出 class、confidence、bounding box
 *   - 实例分割 (segment)：额外输出 mask 轮廓
 *
 * 使用方式：
 *   1. 将 YOLOv8 模型导出为 ONNX 格式
 *      yolo export model=yolov8n.pt format=onnx
 *   2. 在属性面板中设置 modelPath 指向 ONNX 文件
 *   3. 设置 classNames（逗号分隔的类别名，如 "person,car,dog"）
 *   4. 连接图像输入 → 运行 → 获取检测结果
 *
 * 注意：ONNX Runtime 相关实现细节完全封装在 YOLOv8Impl 中（PIMPL 模式），
 * 头文件不暴露任何 ONNX Runtime 依赖。
 */

#pragma once

#include "../../../src/engine/ITool.h"

#include <vector>
#include <string>
#include <memory>

#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

namespace VisionInspector {

// ============================================================
// 单个检测结果
// ============================================================

struct YoloDetection {
    int classId = -1;                    // 类别索引
    float confidence = 0.0f;             // 置信度 [0, 1]
    cv::Rect2f bbox;                     // 边界框（原始图像坐标）
    std::string className;               // 类别名称
    std::vector<cv::Point2f> maskContour; // 分割轮廓（仅 segmentation 模型）
};

// ============================================================
// YOLOv8Detect 工具
// ============================================================

class YOLOv8Detect : public ITool {
    Q_OBJECT

public:
    explicit YOLOv8Detect(QObject* parent = nullptr);
    ~YOLOv8Detect() override;

    // --- ITool 接口 ---

    QString typeName() const override { return QStringLiteral("YOLOv8Detect"); }
    QString displayName() const override { return QStringLiteral("YOLOv8检测"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override {
        return QStringLiteral("基于YOLOv8 ONNX模型的目标检测与实例分割，支持检测和分割两种模式");
    }

    PropertyDefList propertyDefs() const override;
    QVariant propertyValue(const QString& name) const override;
    void setProperty(const QString& name, const QVariant& value) override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

private:
    // --- PIMPL：隐藏 ONNX Runtime 实现细节 ---
    struct YOLOv8Impl;
    std::unique_ptr<YOLOv8Impl> m;

    friend struct YOLOv8Impl;
};

} // namespace VisionInspector
