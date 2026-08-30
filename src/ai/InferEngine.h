/**
 * @file InferEngine.h
 * @brief AI推理引擎 — ONNX Runtime 封装 (阶段4)
 *
 * 功能:
 *   - 加载 ONNX 模型 (自动解析输入尺寸)
 *   - YOLOv8 检测: letterbox预处理 → 推理 → 解码+NMS → 检测框列表
 *   - 通用推理: 预处理后的张量直接推理 (分类/异常检测等)
 *   - 模型缓存: 相同路径的模型全局只加载一次 (多个工具节点共用)
 *
 * 设计:
 *   - PIMPL: ONNX Runtime 头文件只在 .cpp 中, 上层不感知
 *   - 可选依赖: 工程未集成 ONNX Runtime 时本文件编译为空壳
 * 预处理约定(与训练侧对齐):
 *   letterbox(114灰边) → BGR→RGB → /255 → CHW
 */
#pragma once
#define VI_INFER_V2 1

#include "../utils/Common.h"
#include <vector>
#include <QString>
#include <QList>
#include <QPair>

#ifdef VI_HAS_ONNXRT

namespace VisionInspector {

struct AiDetection {
    float x = 0, y = 0, w = 0, h = 0;   // 图像坐标系下的框(左上角+宽高)
    float score = 0;
    int classId = -1;
    QString className;
};

class InferEngine {
public:
    InferEngine();
    ~InferEngine();

    /** 加载模型 (成功后缓存复用) */
    bool loadModel(const QString& onnxPath, QString* err = nullptr);
    bool isLoaded() const;
    QString modelPath() const;
    int inputWidth() const;
    int inputHeight() const;

    /** 模型输出类型 (加载时自动判定) */
    enum class OutputKind { Unknown, Detection, Classification };
    OutputKind outputKind() const { return m_kind; }

    /** YOLOv8 检测 (输出 [1, 4+nc, anchors] 格式) */
    std::vector<AiDetection> detectYolo(const cv::Mat& bgr,
                                        float confThreshold = 0.25f,
                                        float iouThreshold = 0.45f,
                                        QString* err = nullptr);

    /** 分类推理: softmax概率, 结果按分数降序 [{类别名,分数}...] */
    bool classify(const cv::Mat& bgr, QList<QPair<QString, float>>& results, QString* err = nullptr);

    /** 类别名查询 (检测/分类通用), 无则返回 "classN" */
    QString className(int classId) const;

    /** 通用推理: 输入BGR图按模型输入尺寸缩放+归一化(可选letterbox), 返回第一个输出张量 */
    bool run(const cv::Mat& bgr, std::vector<float>& output,
             std::vector<int64_t>& outShape, QString* err = nullptr);

    /** 全局模型缓存: 相同路径共享一个Session */
    static std::shared_ptr<InferEngine> acquire(const QString& onnxPath, QString* err = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    OutputKind m_kind = OutputKind::Unknown;
};

} // namespace VisionInspector
#endif // VI_HAS_ONNXRT
