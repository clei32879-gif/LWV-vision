/**
 * @file AIDetection.cpp
 * @brief AI检测工具节点 — ONNX模型推理 (YOLOv8检测)
 *
 * 工作方式:
 *   1. 按模型路径加载 (全局缓存, 多节点共享一次加载)
 *   2. letterbox预处理 → 推理 → 解码+NMS
 *   3. 检出目标即NG(默认, 可配), 结果键 det0_class/score/x/y/w/h...
 * 模型路径解析: 绝对路径 → appDir相对 → 工作目录相对
 */
#include "AIDetection.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/LicenseManager.h"
#ifdef VI_HAS_ONNXRT
#include "../../../src/ai/InferEngine.h"
#endif
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QVariantMap>

namespace VisionInspector {

// COCO 80类名 (预训练模型; 自训缺陷模型在模型旁放classes.txt可覆盖)
static const char* kCocoNames[] = {
    "person","bicycle","car","motorcycle","airplane","bus","train","truck","boat",
    "traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat",
    "dog","horse","sheep","cow","elephant","bear","zebra","giraffe","backpack",
    "umbrella","handbag","tie","suitcase","frisbee","skis","snowboard","sports ball",
    "kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket",
    "bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple",
    "sandwich","orange","broccoli","carrot","hot dog","pizza","donut","cake",
    "chair","couch","potted plant","bed","dining table","toilet","tv","laptop",
    "mouse","remote","keyboard","cell phone","microwave","oven","toaster","sink",
    "refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"};

static QString resolveModelPath(const QString& path) {
    const QFileInfo fi(path);
    if (fi.isAbsolute() && fi.exists()) return path;
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString& base : {appDir, appDir + "/models", appDir + "/testdata",
                                QCoreApplication::applicationDirPath() + "/../testdata"}) {
        const QString cand = base + "/" + path;
        if (QFileInfo::exists(cand)) return cand;
    }
    return path;
}

PropertyDefList AIDetection::propertyDefs() const {
    return {
        PropertyDef::stringProp("modelPath", "模型文件(onnx)", "models/yolov8n.onnx", "模型"),
        PropertyDef::doubleProp("confThreshold", "置信度阈值", 0.25, 0.01, 1.0, "检测"),
        PropertyDef::doubleProp("iouThreshold", "NMS重叠阈值", 0.45, 0.1, 1.0, "检测"),
        PropertyDef::stringProp("goodClasses", "良品类别(逗号分)", "良品", "判定"),
        PropertyDef::doubleProp("minScore", "最低置信度", 0.5, 0.01, 1.0, "判定"),
        // 缺陷检测场景: 检出(任一检测框)即判定NG; 勾选后忽略goodClasses
        PropertyDef::boolProp("detectionIsNG", "检出即NG(缺陷检测)", false, "判定"),
        PropertyDef::stringProp("targetClasses", "目标类别(逗号分,空=全部)", QString(), "判定"),
        PropertyDef::intProp("minCount", "最少目标数(0=不限)", 0, 0, 9999, "判定"),
        PropertyDef::intProp("maxCount", "最多目标数(0=不限)", 0, 0, 9999, "判定"),
    };
}

bool AIDetection::execute(ToolContext& context) {
    // 授权门禁: AI模块未授权时停用推理 (试用期/有效授权内全开)
    if (!LicenseManager::instance().moduleEnabled(QStringLiteral("ai"))) {
        setStatus(ToolStatus::NG);
        setResultData("error", QStringLiteral("AI模块未授权 — 帮助→关于 中导入授权文件"));
        return false;
    }
#ifndef VI_HAS_ONNXRT
    setResultData("error", "未集成ONNX Runtime, AI功能不可用");
    setStatus(ToolStatus::NG);
    return false;
#else
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        setResultData("error", "输入图像为空"); setStatus(ToolStatus::NG); return false;
    }

    const QString modelPath = resolveModelPath(propertyValue("modelPath").toString());
    QString err;
    auto engine = InferEngine::acquire(modelPath, &err);
    if (!engine) {
        setResultData("error", "模型加载失败: " + err);
        setStatus(ToolStatus::NG);
        return false;
    }

    m_boxes.clear();
    m_lastOk = false;

    // ===== 分类模型分支 (输出[1,nc]) =====
    if (engine->outputKind() == InferEngine::OutputKind::Classification) {
        QList<QPair<QString, float>> results;
        if (!engine->classify(*input, results, &err)) {
            setResultData("error", err);
            setStatus(ToolStatus::NG);
            return false;
        }
        const float minScore = (float)propertyValue("minScore").toDouble();
        for (int i = 0; i < results.size() && i < 5; ++i) {
            setResultData(QString("prob%1_class").arg(i), results[i].first);
            setResultData(QString("prob%1_score").arg(i), results[i].second);
        }
        const QString topClass = results.isEmpty() ? QString() : results[0].first;
        const float topScore = results.isEmpty() ? 0.f : results[0].second;
        setResultData("class", topClass);
        setResultData("score", topScore);
        // 良品类别列表(逗号分) — top1在列表内=OK, 否则NG
        const QStringList goodList = propertyValue("goodClasses").toString()
                                         .split(',', Qt::SkipEmptyParts);
        const float minS = minScore;
        bool pass = false;
        for (const QString& g : goodList)
            if (topClass.trimmed() == g.trimmed() && topScore >= minS) { pass = true; break; }
        setResultData("pass", pass);
        setStatus(pass ? ToolStatus::OK : ToolStatus::NG);
        m_lastOk = true;
        return pass;
    }

    // ===== 检测模型分支 (输出[1,4+nc,anchors]) =====
    QElapsedTimer inferTimer; // 单帧推理总耗时
    inferTimer.start();
    const float conf = (float)propertyValue("confThreshold").toDouble();
    const float iou = (float)propertyValue("iouThreshold").toDouble();
    const std::vector<AiDetection> dets = engine->detectYolo(*input, conf, iou, &err);
    if (!err.isEmpty()) {
        setResultData("error", err);
        setStatus(ToolStatus::NG);
        return false;
    }

    for (size_t i = 0; i < dets.size() && i < 20; ++i) {
        const QString clsName = (dets[i].classId >= 0 && dets[i].classId < 80)
                                    ? QString::fromUtf8(kCocoNames[dets[i].classId])
                                    : QString("class%1").arg(dets[i].classId);
        setResultData(QString("det%1_class").arg(i), clsName);
        setResultData(QString("det%1_score").arg(i), dets[i].score);
        setResultData(QString("det%1_x").arg(i), dets[i].x);
        setResultData(QString("det%1_y").arg(i), dets[i].y);
        setResultData(QString("det%1_w").arg(i), dets[i].w);
        setResultData(QString("det%1_h").arg(i), dets[i].h);
        m_boxes.push_back({dets[i].x, dets[i].y, dets[i].w, dets[i].h,
                           dets[i].classId, dets[i].score});
    }
    setResultData("detectionCount", (int)dets.size());
    m_lastOk = true;

    // 商用判定: 目标类别过滤(忽略非目标类, 如"良品") + 数量区间(漏检/多检都NG)
    QStringList targetClasses;
    for (const QString& s : propertyValue("targetClasses").toString()
             .split(',', Qt::SkipEmptyParts))
        targetClasses << s.trimmed();
    int targetCount = 0;
    if (!targetClasses.isEmpty()) {
        for (const auto& d : dets) {
            const QString clsName = (d.classId >= 0 && d.classId < 80)
                ? QString::fromUtf8(kCocoNames[d.classId])
                : QString("class%1").arg(d.classId);
            if (targetClasses.contains(clsName)) ++targetCount;
        }
    } else {
        targetCount = (int)dets.size();
    }
    setResultData("targetCount", targetCount);
    setResultData("inferenceMs", static_cast<double>(inferTimer.elapsed()));

    // 判定: 检出即NG(缺陷检测场景) / 检出即OK(目标存在场景) + 数量区间
    const bool detIsNG = propertyValue("detectionIsNG").toBool();
    const bool hasDet = !dets.empty();
    const int minCount = propertyValue("minCount").toInt();
    const int maxCount = propertyValue("maxCount").toInt();
    bool pass;
    if (minCount == 0 && maxCount == 0) {
        pass = detIsNG ? !hasDet : hasDet;
    } else {
        const bool countInRange = targetCount >= minCount
                                  && (maxCount == 0 || targetCount <= maxCount);
        pass = detIsNG ? !countInRange : countInRange;
        setResultData("countOk", countInRange);
    }
    setResultData("pass", pass);
    setStatus(pass ? ToolStatus::OK : ToolStatus::NG);
    return pass;
#endif
}

std::vector<QVariant> AIDetection::overlays() const {
    std::vector<QVariant> out;
#ifndef VI_HAS_ONNXRT
    return out;
#else
    if (!m_lastOk) return out;
    if (m_boxes.empty()) {
        // 分类模式: 叠加文本(结果键在resultData, 此处从boxes为空判断)
        return out;
    }
    for (const auto& b : m_boxes) {
        QVariantMap rect;
        rect["type"] = "rect";
        rect["x"] = b.x; rect["y"] = b.y;
        rect["w"] = b.w; rect["h"] = b.h;
        rect["color"] = "#ff4040";
        out.push_back(rect);
        const QString clsName = (b.cls >= 0 && b.cls < 80)
                                    ? QString::fromUtf8(kCocoNames[b.cls])
                                    : QString::number(b.cls);
        QVariantMap text;
        text["type"] = "text";
        text["x"] = b.x; text["y"] = std::max(0.f, b.y - 4);
        text["size"] = 13.0;
        text["text"] = QString("%1 %2%").arg(clsName).arg((int)(b.score * 100));
        text["color"] = "#ffff00";
        out.push_back(text);
    }
    return out;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(AIDetection, "AI检测", VisionInspector::ToolCategory::Special)
