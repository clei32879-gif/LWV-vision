/**
 * @file InferEngine.cpp
 * @brief AI推理引擎实现 (ONNX Runtime)
 */
#include "InferEngine.h"

#ifdef VI_HAS_ONNXRT
#include <onnxruntime_cxx_api.h>
#include <opencv2/imgproc.hpp>
#include <QMutex>
#include <QMap>
#include <cmath>
#include <algorithm>

namespace VisionInspector {

struct InferEngine::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "LWVision"};
    std::unique_ptr<Ort::Session> session;
    QString path;
    int inW = 0, inH = 0;
};

InferEngine::InferEngine() : m_impl(std::make_unique<Impl>()) {}
InferEngine::~InferEngine() = default;

bool InferEngine::loadModel(const QString& onnxPath, QString* err) {
    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(4);
        const std::wstring w = onnxPath.toStdWString();
        m_impl->session = std::make_unique<Ort::Session>(m_impl->env, w.c_str(), opts);
        m_impl->path = onnxPath;
        // 解析输入尺寸 (N,C,H,W)
        Ort::AllocatorWithDefaultOptions alloc;
        auto shape = m_impl->session->GetInputTypeInfo(0)
                         .GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() >= 4) {
            m_impl->inH = (int)shape[2];
            m_impl->inW = (int)shape[3];
        }
        if (err) err->clear();
        return true;
    } catch (const Ort::Exception& e) {
        if (err) *err = QString::fromUtf8(e.what());
        return false;
    }
}

bool InferEngine::isLoaded() const { return m_impl->session != nullptr; }
QString InferEngine::modelPath() const { return m_impl->path; }
int InferEngine::inputWidth() const { return m_impl->inW; }
int InferEngine::inputHeight() const { return m_impl->inH; }

namespace {

/** letterbox缩放到模型输入尺寸, 返回缩放参数(用于坐标映射回原图) */
cv::Mat letterbox(const cv::Mat& bgr, int tw, int th,
                  double& scale, int& padL, int& padT) {
    const double s = std::min((double)tw / bgr.cols, (double)th / bgr.rows);
    const int nw = (int)std::round(bgr.cols * s);
    const int nh = (int)std::round(bgr.rows * s);
    cv::Mat out(tw, th, CV_8UC3, cv::Scalar(114, 114, 114));
    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0, cv::INTER_LINEAR);
    padL = (tw - nw) / 2;
    padT = (th - nh) / 2;
    resized.copyTo(out(cv::Rect(padL, padT, nw, nh)));
    scale = s;
    return out;
}

/** 简易NMS (按分数降序, IoU抑制) */
std::vector<int> nms(std::vector<AiDetection>& dets, float iouThr) {
    std::sort(dets.begin(), dets.end(),
              [](const AiDetection& a, const AiDetection& b) { return a.score > b.score; });
    std::vector<int> keep;
    std::vector<bool> removed(dets.size(), false);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (removed[i]) continue;
        keep.push_back((int)i);
        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (removed[j] || dets[i].classId != dets[j].classId) continue;
            const float x1 = std::max(dets[i].x, dets[j].x);
            const float y1 = std::max(dets[i].y, dets[j].y);
            const float x2 = std::min(dets[i].x + dets[i].w, dets[j].x + dets[j].w);
            const float y2 = std::min(dets[i].y + dets[i].h, dets[j].y + dets[j].h);
            const float inter = std::max(0.f, x2 - x1) * std::max(0.f, y2 - y1);
            const float uni = dets[i].w * dets[i].h + dets[j].w * dets[j].h - inter;
            if (uni > 0 && inter / uni > iouThr) removed[j] = true;
        }
    }
    return keep;
}

} // anonymous namespace

std::vector<AiDetection> InferEngine::detectYolo(const cv::Mat& bgr,
                                                 float confThreshold, float iouThr,
                                                 QString* err) {
    std::vector<AiDetection> out;
    if (!isLoaded()) { if (err) *err = "模型未加载"; return out; }

    double scale = 1.0; int padL = 0, padT = 0;
    const cv::Mat input = letterbox(bgr, m_impl->inW, m_impl->inH, scale, padL, padT);

    // HWC→CHW, BGR→RGB, /255
    const int C = 3, H = m_impl->inH, W = m_impl->inW;
    std::vector<float> tensor(C * H * W);
    for (int c = 0; c < C; ++c)
        for (int y = 0; y < H; ++y) {
            const uchar* row = input.ptr<uchar>(y);
            for (int x = 0; x < W; ++x)
                tensor[c * H * W + y * W + x] = row[x * 3 + (2 - c)] / 255.0f;
        }

    // 推理
    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> inShape{1, (int64_t)C, (int64_t)H, (int64_t)W};
        Ort::Value inTensor = Ort::Value::CreateTensor<float>(
            mem, tensor.data(), tensor.size(), inShape.data(), inShape.size());
        const char* inNames[] = {m_impl->session->GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions()).get()};
        const char* outNames[] = {m_impl->session->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions()).get()};
        auto outTensors = m_impl->session->Run(Ort::RunOptions{nullptr}, inNames, &inTensor, 1, outNames, 1);

        // YOLOv8输出: [1, 4+nc, anchors]
        auto shapeInfo = outTensors[0].GetTensorTypeAndShapeInfo();
        const std::vector<int64_t> oShape = shapeInfo.GetShape();
        const float* data = outTensors[0].GetTensorData<float>();
        const int64_t channels = oShape[1];
        const int64_t anchors = oShape[2];
        const int nc = (int)channels - 4;

        std::vector<AiDetection> dets;
        for (int64_t a = 0; a < anchors; ++a) {
            float maxScore = 0; int maxCls = -1;
            for (int c = 0; c < nc; ++c) {
                const float s = data[(4 + c) * anchors + a];
                if (s > maxScore) { maxScore = s; maxCls = c; }
            }
            if (maxScore < confThreshold) continue;
            const float cxn = data[0 * anchors + a];
            const float cyn = data[1 * anchors + a];
            const float wn = data[2 * anchors + a];
            const float hn = data[3 * anchors + a];
            AiDetection d;
            d.x = (cxn - wn / 2 - padL) / (float)scale;
            d.y = (cyn - hn / 2 - padT) / (float)scale;
            d.w = wn / (float)scale;
            d.h = hn / (float)scale;
            d.score = maxScore;
            d.classId = maxCls;
            dets.push_back(d);
        }
        for (int i : nms(dets, iouThr))
            out.push_back(dets[i]);
        if (err) err->clear();
    } catch (const Ort::Exception& e) {
        if (err) *err = QString::fromUtf8(e.what());
    }
    return out;
}

bool InferEngine::run(const cv::Mat& bgr, std::vector<float>& output,
                      std::vector<int64_t>& outShape, QString* err) {
    if (!isLoaded()) { if (err) *err = "模型未加载"; return false; }
    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(m_impl->inW, m_impl->inH), 0, 0, cv::INTER_LINEAR);
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    std::vector<float> tensor(3 * m_impl->inH * m_impl->inW);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < m_impl->inH; ++y) {
            const uchar* row = rgb.ptr<uchar>(y);
            for (int x = 0; x < m_impl->inW; ++x)
                tensor[c * m_impl->inH * m_impl->inW + y * m_impl->inW + x] =
                    row[x * 3 + c] / 255.0f;
        }
    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> inShape{1, 3, (int64_t)m_impl->inH, (int64_t)m_impl->inW};
        Ort::Value inTensor = Ort::Value::CreateTensor<float>(
            mem, tensor.data(), tensor.size(), inShape.data(), inShape.size());
        const char* inNames[] = {m_impl->session->GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions()).get()};
        const char* outNames[] = {m_impl->session->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions()).get()};
        auto outs = m_impl->session->Run(Ort::RunOptions{nullptr}, inNames, &inTensor, 1, outNames, 1);
        auto info = outs[0].GetTensorTypeAndShapeInfo();
        outShape = info.GetShape();
        const size_t n = info.GetElementCount();
        const float* d = outs[0].GetTensorData<float>();
        output.assign(d, d + n);
        if (err) err->clear();
        return true;
    } catch (const Ort::Exception& e) {
        if (err) *err = QString::fromUtf8(e.what());
        return false;
    }
}

// ============================================================
// 全局模型缓存
// ============================================================
namespace {
QMutex g_modelMutex;
QMap<QString, std::shared_ptr<InferEngine>> g_modelCache;
}

std::shared_ptr<InferEngine> InferEngine::acquire(const QString& onnxPath, QString* err) {
    QMutexLocker locker(&g_modelMutex);
    auto it = g_modelCache.find(onnxPath);
    if (it != g_modelCache.end()) return it.value();
    auto engine = std::make_shared<InferEngine>();
    if (!engine->loadModel(onnxPath, err)) {
        if (err && err->isEmpty()) *err = "模型加载失败";
        return nullptr;
    }
    g_modelCache.insert(onnxPath, engine);
    return engine;
}

} // namespace VisionInspector
#endif // VI_HAS_ONNXRT
