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
#include <QJsonDocument>
#include <QFileInfo>
#include <QFile>
#include <QJsonObject>

namespace VisionInspector {

struct InferEngine::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "LWVision"};
    std::unique_ptr<Ort::Session> session;
    QString path;
    int inW = 0, inH = 0;
    QMap<int, QString> classNames;   // 类别索引 -> 名称 (分类模型)
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
        const auto inShape = m_impl->session->GetInputTypeInfo(0)
                         .GetTensorTypeAndShapeInfo().GetShape();
        if (inShape.size() >= 4) {
            m_impl->inH = (int)inShape[2];
            m_impl->inW = (int)inShape[3];
        }
        // 输出类型决定模型种类: [1,4+nc,anchors]=检测; [1,nc]=分类
        const auto outShape = m_impl->session->GetOutputTypeInfo(0)
                         .GetTensorTypeAndShapeInfo().GetShape();
        if (outShape.size() >= 3)
            m_kind = OutputKind::Detection;
        else if (outShape.size() <= 2)
            m_kind = OutputKind::Classification;
        // 类别名: ultralytics导出的metadata "names" (JSON dict)
        m_impl->classNames.clear();
        try {
            auto meta = m_impl->session->GetModelMetadata();
            Ort::AllocatorWithDefaultOptions alloc;
            auto namesStr = meta.LookupCustomMetadataMapAllocated("names", alloc);
            if (namesStr) {
                const QJsonDocument doc = QJsonDocument::fromJson(
                    QString::fromUtf8(namesStr.get()).toUtf8());
                const QJsonObject obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it)
                    m_impl->classNames.insert(it.key().toInt(), it.value().toString());
            }
        } catch (...) {}
        // 元数据缺names时: 读模型旁classes.txt (兼容: classes.txt 或 <模型名>_classes.txt)
        if (m_impl->classNames.isEmpty()) {
            const QFileInfo modelFi(onnxPath);
            QString clsFile = modelFi.absolutePath() + "/classes.txt";
            if (!QFile::exists(clsFile)) {
                const QString alt = modelFi.absolutePath() + "/"
                                    + modelFi.completeBaseName() + "_classes.txt";
                if (QFile::exists(alt)) clsFile = alt;
            }
            QFile f(clsFile);
            if (f.open(QIODevice::ReadOnly)) {
                int idx = 0;
                while (!f.atEnd()) {
                    const QString line = QString::fromUtf8(f.readLine()).trimmed();
                    if (!line.isEmpty()) m_impl->classNames.insert(idx++, line);
                }
            }
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
        auto inName = m_impl->session->GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        auto outName = m_impl->session->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        const char* inNames[] = {inName.get()};
        const char* outNames[] = {outName.get()};
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
        auto inName = m_impl->session->GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        auto outName = m_impl->session->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        const char* inNames[] = {inName.get()};
        const char* outNames[] = {outName.get()};
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

bool InferEngine::classify(const cv::Mat& bgr, QList<QPair<QString, float>>& results,
                           QString* err) {
    results.clear();
    if (!isLoaded()) { if (err) *err = "模型未加载"; return false; }

    // 与ultralytics一致的分类预处理: 短边等比缩放到输入尺寸 -> 中心裁剪 -> RGB/255
    const int T = m_impl->inH;   // 方形输入 (224)
    const double sc = (double)T / std::min(bgr.cols, bgr.rows);
    const int nw = std::max(T, (int)std::round(bgr.cols * sc));
    const int nh = std::max(T, (int)std::round(bgr.rows * sc));
    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0,
               sc < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);   // 下采样抗锯齿
    const int cropX = (nw - T) / 2, cropY = (nh - T) / 2;
    cv::Mat rgb;
    cv::cvtColor(resized(cv::Rect(cropX, cropY, T, T)), rgb, cv::COLOR_BGR2RGB);

    // HWC->CHW /255
    std::vector<float> tensor(3 * T * T);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < T; ++y) {
            const uchar* row = rgb.ptr<uchar>(y);
            for (int x = 0; x < T; ++x)
                tensor[c * T * T + y * T + x] = row[x * 3 + c] / 255.0f;
        }

    std::vector<float> out;
    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> inShape{1, 3, (int64_t)T, (int64_t)T};
        Ort::Value inTensor = Ort::Value::CreateTensor<float>(
            mem, tensor.data(), tensor.size(), inShape.data(), inShape.size());
        auto inName = m_impl->session->GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        auto outName = m_impl->session->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        const char* inNames[] = {inName.get()};
        const char* outNames[] = {outName.get()};
        auto outs = m_impl->session->Run(Ort::RunOptions{nullptr}, inNames, &inTensor, 1, outNames, 1);
        const size_t n = outs[0].GetTensorTypeAndShapeInfo().GetElementCount();
        const float* d = outs[0].GetTensorData<float>();
        out.assign(d, d + n);
    } catch (const Ort::Exception& e) {
        if (err) *err = QString::fromUtf8(e.what());
        return false;
    }
    if (out.empty()) { if (err) *err = "分类输出为空"; return false; }

    // softmax — 但ultralytics分类导出的输出已是概率(和≈1), 二次softmax会压平分布
    double sumRaw = 0;
    for (float v : out) sumRaw += v;
    if (std::fabs(sumRaw - 1.0) > 0.01) {
        double maxV = *std::max_element(out.begin(), out.end());
        double sum = 0;
        for (auto& v : out) { v = std::exp(v - maxV); sum += v; }
        for (auto& v : out) v = (float)(v / sum);
    }
    // 填充结果 (类别名从模型元数据/classes.txt)
    for (size_t i = 0; i < out.size(); ++i) {
        const QString name = m_impl->classNames.value((int)i,
                                                      QString("class%1").arg(i));
        results.append({name, out[i]});
    }
    std::sort(results.begin(), results.end(),
              [](const QPair<QString, float>& a, const QPair<QString, float>& b) {
                  return a.second > b.second;
              });
    return true;
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
