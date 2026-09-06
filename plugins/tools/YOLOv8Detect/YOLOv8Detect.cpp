/**
 * @file YOLOv8Detect.cpp
 * @brief YOLOv8 ONNX 推理实现（PIMPL 模式）
 *
 * 完整实现：
 *   - ONNX Runtime C++ API 模型加载与推理
 *   - 预处理：Letterbox、归一化、BGR→RGB、HWC→CHW
 *   - 后处理：置信度过滤、NMS、坐标映射
 *   - 分割掩码提取（segmentation 模型）
 *   - Overlay 可视化（边界框 + 标签 + 掩码轮廓）
 */

#include "YOLOv8Detect.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/LicenseManager.h"
#include <QElapsedTimer>

#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/geometry.hpp>

// ONNX Runtime C++ API
#ifdef VI_HAS_ONNXRT
#include <onnxruntime_cxx_api.h>
#endif

#endif // VI_HAS_OPENCV

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace VisionInspector {

// ============================================================
// PIMPL 实现结构（所有 ONNX Runtime 和内部状态）
// ============================================================

struct YOLOv8Detect::YOLOv8Impl {

    // --- ONNX Runtime 对象 ---
    Ort::Env env{nullptr};
    std::unique_ptr<Ort::Session> session;
    bool modelLoaded = false;
    bool isSegmentModel = false;

    // --- 输入/输出名称 ---
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;

    // --- 属性 ---
    QString modelPath;
    float confidenceThreshold = 0.5f;
    float nmsThreshold = 0.45f;
    int inputSize = 640;
    QString classNamesStr;
    QString modelType = "detect";

    // --- 运行时缓存 ---
    std::vector<std::string> classNames;
    std::vector<YoloDetection> lastDetections;

    // Letterbox 参数
    float scale = 1.0f;
    int padX = 0;
    int padY = 0;
    int originalWidth = 0;
    int originalHeight = 0;

    // ============================================================
    // 模型管理
    // ============================================================

    void releaseModel() {
        session.reset();
        modelLoaded = false;
        isSegmentModel = false;
        inputNames.clear();
        outputNames.clear();
    }

    bool loadModel() {
        if (modelLoaded) return true;
        if (modelPath.isEmpty()) return false;

        releaseModel();

        try {
            // 创建 Env（使用已有的 ORT 环境或新建）
            env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "YOLOv8Detect");

            // 配置 Session 选项
            Ort::SessionOptions sessionOptions;
            sessionOptions.SetIntraOpNumThreads(4);
            sessionOptions.SetGraphOptimizationLevel(
                GraphOptimizationLevel::ORT_ENABLE_ALL);

            // 加载模型 (ORT 1.18: 路径用 wstring, 命名用 Allocated 接口)
            const std::wstring modelPathW = modelPath.toStdWString();
            session = std::make_unique<Ort::Session>(
                env, modelPathW.c_str(), sessionOptions);

            // 获取分配器
            Ort::AllocatorWithDefaultOptions allocator;

            // 读取输入节点名
            inputNames.clear();
            size_t numInputNodes = session->GetInputCount();
            for (size_t i = 0; i < numInputNodes; ++i) {
                auto name = session->GetInputNameAllocated(i, allocator);
                inputNames.push_back(name.get());
            }

            // 读取输出节点名
            outputNames.clear();
            size_t numOutputNodes = session->GetOutputCount();
            for (size_t i = 0; i < numOutputNodes; ++i) {
                auto name = session->GetOutputNameAllocated(i, allocator);
                outputNames.push_back(name.get());
            }

            isSegmentModel = (modelType == "segment") || (numOutputNodes >= 2);
            modelLoaded = true;
            return true;
        }
        catch (const Ort::Exception& e) {
            releaseModel();
            return false;
        }
    }

    // ============================================================
    // 辅助数学函数
    // ============================================================

    static float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

    static float computeIOU(const YoloDetection& a, const YoloDetection& b) {
        float x1 = std::max(a.bbox.x, b.bbox.x);
        float y1 = std::max(a.bbox.y, b.bbox.y);
        float x2 = std::min(a.bbox.x + a.bbox.width, b.bbox.x + b.bbox.width);
        float y2 = std::min(a.bbox.y + a.bbox.height, b.bbox.y + b.bbox.height);

        float interArea = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
        float areaA = a.bbox.width * a.bbox.height;
        float areaB = b.bbox.width * b.bbox.height;
        float unionArea = areaA + areaB - interArea;

        return (unionArea > 1e-6f) ? interArea / unionArea : 0.0f;
    }

    // ============================================================
    // NMS（非极大值抑制）
    // ============================================================

    std::vector<YoloDetection> applyNMS(
        const std::vector<YoloDetection>& detections) const
    {
        if (detections.empty()) return {};

        // 按置信度降序排列
        std::vector<size_t> indices(detections.size());
        for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
        std::sort(indices.begin(), indices.end(),
            [&](size_t a, size_t b) {
                return detections[a].confidence > detections[b].confidence;
            });

        std::vector<YoloDetection> result;
        std::vector<bool> suppressed(detections.size(), false);

        for (size_t i = 0; i < indices.size(); ++i) {
            size_t idx = indices[i];
            if (suppressed[idx]) continue;

            result.push_back(detections[idx]);

            // 抑制同类别高重叠检测框
            for (size_t j = i + 1; j < indices.size(); ++j) {
                size_t otherIdx = indices[j];
                if (suppressed[otherIdx]) continue;

                if (detections[idx].classId == detections[otherIdx].classId) {
                    float iou = computeIOU(detections[idx], detections[otherIdx]);
                    if (iou > nmsThreshold) {
                        suppressed[otherIdx] = true;
                    }
                }
            }
        }

        return result;
    }

    // ============================================================
    // 预处理：Letterbox + BGR→RGB + 归一化 + HWC→CHW
    // ============================================================

    cv::Mat preprocess(const cv::Mat& image) {
        originalWidth = image.cols;
        originalHeight = image.rows;

        // 1. Letterbox：保持宽高比缩放并居中填充到 square
        float r = std::min(
            static_cast<float>(inputSize) / image.cols,
            static_cast<float>(inputSize) / image.rows);
        scale = r;

        int newW = static_cast<int>(std::round(image.cols * r));
        int newH = static_cast<int>(std::round(image.rows * r));

        cv::Mat resized;
        cv::resize(image, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

        padX = (inputSize - newW) / 2;
        padY = (inputSize - newH) / 2;

        // 灰色填充 (114, 114, 114) — YOLO 标准填充色
        cv::Mat letterbox(inputSize, inputSize, CV_8UC3, cv::Scalar(114, 114, 114));
        resized.copyTo(letterbox(cv::Rect(padX, padY, newW, newH)));

        // 2. BGR → RGB
        cv::Mat rgb;
        cv::cvtColor(letterbox, rgb, cv::COLOR_BGR2RGB);

        // 3. uint8 → float32 + 归一化 [0, 1]
        cv::Mat normalized;
        rgb.convertTo(normalized, CV_32FC3, 1.0 / 255.0);

        // 4. HWC → CHW
        std::vector<cv::Mat> channels(3);
        cv::split(normalized, channels); // R, G, B 各 H×W

        // 5. 拼接为连续 CHW 内存 [3, H, W]
        cv::Mat chw(3, inputSize * inputSize, CV_32FC1);
        for (int c = 0; c < 3; ++c) {
            std::memcpy(
                chw.ptr<float>(c),
                channels[c].ptr<float>(),
                static_cast<size_t>(inputSize * inputSize) * sizeof(float));
        }

        return chw;
    }

    // ============================================================
    // 边界框解码：从 letterbox 坐标映射回原始图像坐标
    // ============================================================

    cv::Rect2f decodeBBox(float cx, float cy, float w, float h,
                           int origW, int origH) const
    {
        // cx, cy, w, h 在 [0, 1] 范围内（相对于 inputSize 归一化）
        // 转为 letterbox 上的像素坐标
        float x1_lb = (cx - w / 2.0f) * inputSize;
        float y1_lb = (cy - h / 2.0f) * inputSize;
        float x2_lb = (cx + w / 2.0f) * inputSize;
        float y2_lb = (cy + h / 2.0f) * inputSize;

        // 去除 letterbox padding
        float invScale = 1.0f / scale;
        float x1 = (x1_lb - padX) * invScale;
        float y1 = (y1_lb - padY) * invScale;
        float x2 = (x2_lb - padX) * invScale;
        float y2 = (y2_lb - padY) * invScale;

        // 裁剪到图像边界内
        x1 = std::max(0.0f, std::min(x1, static_cast<float>(origW)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(origH)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(origW)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(origH)));

        return cv::Rect2f(x1, y1, x2 - x1, y2 - y1);
    }

    // ============================================================
    // 后处理 - 检测模型
    // ============================================================

    std::vector<YoloDetection> postprocessDetect(
        const float* data, int64_t numChannels, int64_t numPredictions,
        int origW, int origH)
    {
        std::vector<YoloDetection> detections;
        int numClasses = static_cast<int>(numChannels) - 4;
        if (numClasses <= 0) return detections;

        for (int64_t i = 0; i < numPredictions; ++i) {
            // 找最大类别分数
            float maxScore = 0.0f;
            int bestClass = -1;

            for (int c = 0; c < numClasses; ++c) {
                float score = data[(4 + c) * numPredictions + i];
                // 如果值不在 [0, 1] 范围，应用 sigmoid
                if (score < 0.0f || score > 1.0f) score = sigmoid(score);
                if (score > maxScore) {
                    maxScore = score;
                    bestClass = c;
                }
            }

            if (maxScore < confidenceThreshold) continue;

            // 提取 bbox（cx, cy, w, h 归一化到 [0,1]）
            float cx = data[0 * numPredictions + i];
            float cy = data[1 * numPredictions + i];
            float w  = data[2 * numPredictions + i];
            float h  = data[3 * numPredictions + i];

            // 兼容不同导出格式：超出 [0,1] 时应用 sigmoid
            if (cx < 0.0f || cx > 1.0f) cx = sigmoid(cx);
            if (cy < 0.0f || cy > 1.0f) cy = sigmoid(cy);
            if (w < 0.0f || w > 1.0f)   w  = sigmoid(w);
            if (h < 0.0f || h > 1.0f)   h  = sigmoid(h);

            cv::Rect2f bbox = decodeBBox(cx, cy, w, h, origW, origH);
            if (bbox.width < 1.0f || bbox.height < 1.0f) continue;

            YoloDetection det;
            det.classId = bestClass;
            det.confidence = maxScore;
            det.bbox = bbox;
            if (bestClass >= 0 && bestClass < static_cast<int>(classNames.size())) {
                det.className = classNames[bestClass];
            } else {
                det.className = "class_" + std::to_string(bestClass);
            }

            detections.push_back(det);
        }

        return applyNMS(detections);
    }

    // ============================================================
    // 掩码轮廓提取
    // ============================================================

    static std::vector<cv::Point2f> extractMaskContour(
        const float* maskData, int maskW, int maskH)
    {
        cv::Mat maskMat(maskH, maskW, CV_32FC1, const_cast<float*>(maskData));
        cv::Mat binary;
        cv::threshold(maskMat, binary, 0.5, 1.0, cv::THRESH_BINARY);
        binary.convertTo(binary, CV_8UC1, 255);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL,
                         cv::CHAIN_APPROX_SIMPLE);

        if (contours.empty()) return {};

        // 取面积最大的轮廓
        size_t best = 0;
        double bestArea = 0.0;
        for (size_t i = 0; i < contours.size(); ++i) {
            double area = cv::contourArea(contours[i]);
            if (area > bestArea) { bestArea = area; best = i; }
        }

        std::vector<cv::Point2f> result;
        result.reserve(contours[best].size());
        for (const auto& pt : contours[best]) {
            result.emplace_back(
                static_cast<float>(pt.x), static_cast<float>(pt.y));
        }
        return result;
    }

    // ============================================================
    // 后处理 - 分割模型
    // ============================================================

    std::vector<YoloDetection> postprocessSegment(
        const float* out0, int64_t numCh, int64_t numPred,
        const float* out1, int64_t protoCh, int64_t protoH, int64_t protoW,
        int origW, int origH)
    {
        // numCh = 4 + numClasses + maskCoeffs
        int maskCoeffs = static_cast<int>(numCh) - 4
            - std::max(0, static_cast<int>(classNames.size()));
        if (maskCoeffs < 0) maskCoeffs = 32; // 默认 32 个掩码系数

        int numClasses = static_cast<int>(numCh) - 4 - maskCoeffs;
        if (numClasses <= 0) {
            // 回退：当作纯检测处理
            return postprocessDetect(out0, numCh, numPred, origW, origH);
        }

        std::vector<YoloDetection> detections;

        for (int64_t i = 0; i < numPred; ++i) {
            float maxScore = 0.0f;
            int bestClass = -1;

            for (int c = 0; c < numClasses; ++c) {
                float score = out0[(4 + c) * numPred + i];
                if (score < 0.0f || score > 1.0f) score = sigmoid(score);
                if (score > maxScore) { maxScore = score; bestClass = c; }
            }

            if (maxScore < confidenceThreshold) continue;

            float cx = out0[0 * numPred + i];
            float cy = out0[1 * numPred + i];
            float w  = out0[2 * numPred + i];
            float h  = out0[3 * numPred + i];

            if (cx < 0.0f || cx > 1.0f) cx = sigmoid(cx);
            if (cy < 0.0f || cy > 1.0f) cy = sigmoid(cy);
            if (w < 0.0f || w > 1.0f)   w  = sigmoid(w);
            if (h < 0.0f || h > 1.0f)   h  = sigmoid(h);

            cv::Rect2f bbox = decodeBBox(cx, cy, w, h, origW, origH);
            if (bbox.width < 1.0f || bbox.height < 1.0f) continue;

            YoloDetection det;
            det.classId = bestClass;
            det.confidence = maxScore;
            det.bbox = bbox;
            det.className = (bestClass >= 0 && bestClass < static_cast<int>(classNames.size()))
                ? classNames[bestClass]
                : "class_" + std::to_string(bestClass);

            // 掩码处理
            // mask = sigmoid( sum( coeffs[k] * proto[k] ) )
            // 提取掩码系数
            std::vector<float> coeffs(maskCoeffs);
            for (int c = 0; c < maskCoeffs; ++c) {
                coeffs[c] = out0[(4 + numClasses + c) * numPred + i];
            }

            // 计算掩码：coeffs · proto → sigmoid
            std::vector<float> maskData(static_cast<size_t>(protoH * protoW), 0.0f);
            for (int64_t p = 0; p < protoH * protoW; ++p) {
                float sum = 0.0f;
                for (int k = 0; k < maskCoeffs; ++k) {
                    sum += coeffs[k] * out1[k * protoH * protoW + p];
                }
                maskData[p] = sigmoid(sum);
            }

            det.maskContour = extractMaskContour(
                maskData.data(),
                static_cast<int>(protoW),
                static_cast<int>(protoH));

            detections.push_back(det);
        }

        return applyNMS(detections);
    }
};

// ============================================================
// YOLOv8Detect 公共接口实现
// ============================================================

YOLOv8Detect::YOLOv8Detect(QObject* parent)
    : ITool(parent)
    , m(std::make_unique<YOLOv8Impl>())
{
    // 设置默认属性值
    m_properties["modelPath"] = QString();
    m_properties["confidenceThreshold"] = 0.5;
    m_properties["nmsThreshold"] = 0.45;
    m_properties["inputSize"] = 640;
    m_properties["classNames"] = QString();
    m_properties["modelType"] = QStringLiteral("detect");
}

YOLOv8Detect::~YOLOv8Detect()
{
    m->releaseModel();
}

// ------------------------------------------------------------
// 属性定义
// ------------------------------------------------------------

PropertyDefList YOLOv8Detect::propertyDefs() const
{
    return {
        // 模型配置
        PropertyDef::stringProp("modelPath", "模型路径",
            QString(), "模型配置"),
        PropertyDef::enumProp("modelType", "模型类型",
            {"detect", "segment"}, 0, "模型配置"),
        PropertyDef::intProp("inputSize", "输入尺寸",
            640, 320, 1280, "模型配置"),
        PropertyDef::stringProp("classNames", "类别名称",
            QString(), "模型配置"),

        // 检测参数
        PropertyDef::doubleProp("confidenceThreshold", "置信度阈值",
            0.5, 0.01, 1.0, "检测参数"),
        PropertyDef::doubleProp("nmsThreshold", "NMS阈值",
            0.45, 0.01, 1.0, "检测参数"),

        // 判定参数 (商用级: 类别过滤 + 数量区间, 漏检/多检都判NG)
        PropertyDef::stringProp("targetClasses", "目标类别(逗号分,空=全部)",
            QString(), "判定参数"),
        PropertyDef::intProp("minCount", "最少目标数(0=不限)",
            0, 0, 9999, "判定参数"),
        PropertyDef::intProp("maxCount", "最多目标数(0=不限)",
            0, 0, 9999, "判定参数"),
    };
}

// ------------------------------------------------------------
// 属性读写
// ------------------------------------------------------------

QVariant YOLOv8Detect::propertyValue(const QString& name) const
{
    // H-4b: 覆写实现必须走基类锁 (工作线程写 vs UI 线程读不竞态)
    return withStateLock([&]() -> QVariant {
        if (name == "modelPath")           return m->modelPath;
        if (name == "confidenceThreshold")  return m->confidenceThreshold;
        if (name == "nmsThreshold")        return m->nmsThreshold;
        if (name == "inputSize")           return m->inputSize;
        if (name == "classNames")          return m->classNamesStr;
        if (name == "modelType")           return m->modelType;
        // 锁内直读基类属性 (不能调 ITool::propertyValue, 其内部会二次加锁死锁)
        if (m_properties.contains(name))
            return m_properties.value(name);
        for (const auto& def : propertyDefs())
            if (def.name == name)
                return def.defaultValue;
        return QVariant();
    });
}

void YOLOv8Detect::setProperty(const QString& name, const QVariant& value)
{
    // H-4b: 覆写实现必须走基类锁; releaseModel 在锁内完成(路径/参数一致性保证)
    withStateLock([&] {
        if (name == "modelPath") {
            QString newPath = value.toString();
            if (newPath != m->modelPath) {
                m->modelPath = newPath;
                m->releaseModel(); // 路径变更，释放旧模型
            }
        }
        else if (name == "confidenceThreshold") { m->confidenceThreshold = value.toFloat(); }
        else if (name == "nmsThreshold")      { m->nmsThreshold = value.toFloat(); }
        else if (name == "inputSize")         { m->inputSize = value.toInt(); }
        else if (name == "classNames")        {
            m->classNamesStr = value.toString();
            m->classNames.clear();
            QStringList parts = m->classNamesStr.split(",", Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                m->classNames.push_back(part.trimmed().toStdString());
            }
        }
        else if (name == "modelType")         { m->modelType = value.toString(); }
        else { m_properties[name] = value; return; }  // 锁内直写基类属性

        m_properties[name] = value;
    });
}

// ------------------------------------------------------------
// 序列化
// ------------------------------------------------------------

QJsonObject YOLOv8Detect::toJson() const
{
    QJsonObject json = ITool::toJson();
    json["modelPath"] = m->modelPath;
    json["confidenceThreshold"] = m->confidenceThreshold;
    json["nmsThreshold"] = m->nmsThreshold;
    json["inputSize"] = m->inputSize;
    json["classNames"] = m->classNamesStr;
    json["modelType"] = m->modelType;
    return json;
}

void YOLOv8Detect::fromJson(const QJsonObject& json)
{
    ITool::fromJson(json);
    if (json.contains("modelPath"))           m->modelPath = json["modelPath"].toString();
    if (json.contains("confidenceThreshold"))  m->confidenceThreshold = static_cast<float>(json["confidenceThreshold"].toDouble());
    if (json.contains("nmsThreshold"))        m->nmsThreshold = static_cast<float>(json["nmsThreshold"].toDouble());
    if (json.contains("inputSize"))           m->inputSize = json["inputSize"].toInt();
    if (json.contains("classNames"))          m->classNamesStr = json["classNames"].toString();
    if (json.contains("modelType"))           m->modelType = json["modelType"].toString();

    m->classNames.clear();
    QStringList parts = m->classNamesStr.split(",", Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        m->classNames.push_back(part.trimmed().toStdString());
    }
}

// ------------------------------------------------------------
// 执行（核心入口）
// ------------------------------------------------------------

bool YOLOv8Detect::execute(ToolContext& context)
{
    // 授权门禁: AI模块未授权时停用推理 (试用期/有效授权内全开)
    if (!LicenseManager::instance().moduleEnabled(QStringLiteral("ai"))) {
        setStatus(ToolStatus::NG);
        setResultData("error", QStringLiteral("AI模块未授权 — 帮助→关于 中导入授权文件"));
        setResultData("found", false);
        setResultData("count", 0);
        return false;
    }
#if defined(VI_HAS_OPENCV) && defined(VI_HAS_ONNXRT)
    m->lastDetections.clear();
    setStatus(ToolStatus::Running);

    // 1. 获取输入图像
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        setResultData("error", QStringLiteral("输入图像为空"));
        setResultData("found", false);
        setResultData("count", 0);
        setStatus(ToolStatus::NG);
        return false;
    }

    // 2. 加载模型
    if (!m->loadModel()) {
        setResultData("error", QStringLiteral("模型加载失败: %1").arg(m->modelPath));
        setResultData("found", false);
        setResultData("count", 0);
        setStatus(ToolStatus::Error);
        return false;
    }

    try {
        QElapsedTimer inferTimer; // 单帧总耗时: 预处理+推理+后处理
        inferTimer.start();
        // 3. 预处理
        cv::Mat preprocessed = m->preprocess(*input);

        // 4. 创建输入张量 [1, 3, H, W]
        std::vector<int64_t> inputShape = {
            1, 3,
            static_cast<int64_t>(m->inputSize),
            static_cast<int64_t>(m->inputSize)
        };
        size_t tensorSize = 1 * 3 * m->inputSize * m->inputSize;

        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            const_cast<float*>(preprocessed.ptr<float>()),
            tensorSize,
            inputShape.data(),
            inputShape.size());

        // 5. 运行推理
        std::vector<const char*> inNamesCStr;
        std::vector<const char*> outNamesCStr;
        for (const auto& name : m->inputNames)  inNamesCStr.push_back(name.c_str());
        for (const auto& name : m->outputNames) outNamesCStr.push_back(name.c_str());

        auto outputTensors = m->session->Run(
            Ort::RunOptions{nullptr},
            inNamesCStr.data(), &inputTensor, 1,
            outNamesCStr.data(), m->outputNames.size());

        if (outputTensors.empty()) {
            setResultData("error", QStringLiteral("模型输出为空"));
            setResultData("found", false);
            setResultData("count", 0);
            setStatus(ToolStatus::Error);
            return false;
        }

        // 6. 后处理
        auto shape0 = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        if (shape0.size() < 3) {
            setResultData("error",
                QStringLiteral("输出格式不支持，期望 [1, channels, predictions]"));
            setResultData("found", false);
            setResultData("count", 0);
            setStatus(ToolStatus::Error);
            return false;
        }

        int64_t numCh = shape0[1];
        int64_t numPred = shape0[2];
        const float* out0 = outputTensors[0].GetTensorMutableData<float>();

        if (m->isSegmentModel && outputTensors.size() >= 2) {
            // 分割模型
            auto shape1 = outputTensors[1].GetTensorTypeAndShapeInfo().GetShape();
            int64_t pCh = shape1.size() >= 3 ? shape1[1] : 32;
            int64_t pH  = shape1.size() >= 3 ? shape1[2] : 160;
            int64_t pW  = shape1.size() >= 3 ? shape1[3] : 160;
            const float* out1 = outputTensors[1].GetTensorMutableData<float>();

            m->lastDetections = m->postprocessSegment(
                out0, numCh, numPred,
                out1, pCh, pH, pW,
                m->originalWidth, m->originalHeight);
        } else {
            // 检测模型
            m->lastDetections = m->postprocessDetect(
                out0, numCh, numPred,
                m->originalWidth, m->originalHeight);
        }

        // 7. 写入结果
        // 类别过滤 (商用判定): 目标类别之外的检出不计入判定 — 如忽略"良品"类
        QStringList targetClasses;
        for (const QString& s : propertyValue("targetClasses").toString()
                 .split(',', Qt::SkipEmptyParts))
            targetClasses << s.trimmed();
        std::vector<YoloDetection> filtered;
        for (const auto& det : m->lastDetections) {
            if (targetClasses.isEmpty() ||
                targetClasses.contains(QString::fromStdString(det.className)))
                filtered.push_back(det);
        }

        int count = static_cast<int>(filtered.size());
        setResultData("inferenceMs", static_cast<double>(inferTimer.elapsed()));
        setResultData("count", count);
        setResultData("found", count > 0);
        setResultData("rawCount", static_cast<int>(m->lastDetections.size()));

        // JSON 格式检测结果 + 逐目标平铺键 (defN_*, 供数据判定/显示逐个引用)
        QJsonArray detectionsJson;
        int flat = 0;
        for (const auto& det : filtered) {
            QJsonObject obj;
            obj["classId"] = det.classId;
            obj["className"] = QString::fromStdString(det.className);
            obj["confidence"] = det.confidence;
            obj["x"] = det.bbox.x;
            obj["y"] = det.bbox.y;
            obj["width"] = det.bbox.width;
            obj["height"] = det.bbox.height;
            detectionsJson.append(obj);
            if (flat < 50) {
                setResultData(QString("def%1_class").arg(flat),
                              QString::fromStdString(det.className));
                setResultData(QString("def%1_confidence").arg(flat), det.confidence);
                setResultData(QString("def%1_x").arg(flat), det.bbox.x);
                setResultData(QString("def%1_y").arg(flat), det.bbox.y);
                setResultData(QString("def%1_w").arg(flat), det.bbox.width);
                setResultData(QString("def%1_h").arg(flat), det.bbox.height);
                ++flat;
            }
        }
        setResultData("detections", detectionsJson);

        // 类别统计 (过滤后)
        QMap<int, int> classCount;
        for (const auto& det : filtered) {
            classCount[det.classId]++;
        }
        QJsonObject classStats;
        for (auto it = classCount.begin(); it != classCount.end(); ++it) {
            QString key = (it.key() >= 0 && it.key() < static_cast<int>(m->classNames.size()))
                ? QString::fromStdString(m->classNames[it.key()])
                : QString("class_%1").arg(it.key());
            classStats[key] = it.value();
        }
        setResultData("classStats",
            QJsonDocument(classStats).toJson(QJsonDocument::Compact));

        if (count > 0) {
            setResultData("topClass", filtered[0].classId);
            setResultData("topConfidence", filtered[0].confidence);
        }

        // 数量区间判定 (商用级): 漏检/多检都NG; min=max=0 保持旧行为(检出即OK)
        const int minCount = propertyValue("minCount").toInt();
        const int maxCount = propertyValue("maxCount").toInt();
        bool ok;
        if (minCount == 0 && maxCount == 0)
            ok = count > 0;
        else {
            ok = count >= minCount && (maxCount == 0 || count <= maxCount);
            setResultData("countOk", ok);
        }
        setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
        return ok;
    }
    catch (const Ort::Exception& e) {
        setResultData("error", QString("ONNX Runtime 异常: %1").arg(e.what()));
        setResultData("found", false);
        setResultData("count", 0);
        setStatus(ToolStatus::Error);
        return false;
    }
    catch (const std::exception& e) {
        setResultData("error", QString("异常: %1").arg(e.what()));
        setResultData("found", false);
        setResultData("count", 0);
        setStatus(ToolStatus::Error);
        return false;
    }

#else
    setResultData("error", QStringLiteral("需要 OpenCV 和 ONNX Runtime 库"));
    setResultData("found", false);
    setResultData("count", 0);
    setStatus(ToolStatus::Error);
    return false;
#endif
}

// ------------------------------------------------------------
// Overlay 可视化
// ------------------------------------------------------------

std::vector<QVariant> YOLOv8Detect::overlays() const
{
    std::vector<QVariant> result;

    // 按类别区分的颜色表
    static const QStringList classColors = {
        "#FF0000", "#00FF00", "#0000FF", "#FFFF00",
        "#FF00FF", "#00FFFF", "#FF8800", "#8800FF",
        "#FF0088", "#0088FF", "#88FF00", "#FF4444",
        "#44FF44", "#4444FF", "#FFAA00", "#AA00FF"
    };

    for (const auto& det : m->lastDetections) {
        QString color = classColors[det.classId % classColors.size()];

        // 边界框
        QJsonObject box;
        box["type"] = QStringLiteral("box");
        box["x"] = det.bbox.x;
        box["y"] = det.bbox.y;
        box["width"] = det.bbox.width;
        box["height"] = det.bbox.height;

        QString label = QString::fromStdString(det.className)
            + QString(" %1%").arg(static_cast<int>(det.confidence * 100));
        box["label"] = label;
        box["labelColor"] = QStringLiteral("#00FF00");
        box["strokeColor"] = color;
        box["strokeWidth"] = 2;

        result.push_back(QVariant::fromValue(box));

        // 分割轮廓
        if (!det.maskContour.empty()) {
            QJsonObject poly;
            poly["type"] = QStringLiteral("polygon");
            QJsonArray points;
            for (const auto& pt : det.maskContour) {
                QJsonObject p;
                p["x"] = pt.x;
                p["y"] = pt.y;
                points.append(p);
            }
            poly["points"] = points;
            poly["strokeColor"] = color;
            poly["fillColor"] = color + QStringLiteral("44");
            poly["strokeWidth"] = 1;

            result.push_back(QVariant::fromValue(poly));
        }
    }

    return result;
}

} // namespace VisionInspector

// ============================================================
// 工具注册
// ============================================================

VI_REGISTER_TOOL(YOLOv8Detect, "YOLOv8检测", VisionInspector::ToolCategory::Detection)
