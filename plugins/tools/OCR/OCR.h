/**
 * @file OCR.h - 字符识别工具 (真实实现: 字符分割 + 归一化模板匹配)
 *
 * 原理 (印刷体, 无外部依赖):
 *   教导模式: 输入期望文本 → 每个字符分割出来归一化成16x24模板存文件
 *   识别模式: 图像二值化 → 连通域分割字符 → 归一化 → 与模板集匹配 → 输出文本
 */
#pragma once
#include "../../../src/engine/ITool.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#endif
#include <QMap>
#include <QList>

namespace VisionInspector {
class OCR : public ITool {
    Q_OBJECT
public:
    QString typeName() const override { return QStringLiteral("OCR"); }
    QString displayName() const override { return QStringLiteral("字符识别"); }
    ToolCategory category() const override { return ToolCategory::Detection; }
    QString description() const override {
        return QStringLiteral("字符识别: 先教导(输入期望文本学习模板), 后识别输出文本");
    }
    PropertyDefList propertyDefs() const override;
    bool execute(ToolContext& context) override;
    std::vector<QVariant> overlays() const override;

private:
#ifdef VI_HAS_OPENCV
    /** 字符分割: 返回按x排序的字符二值图列表(与边界框列表) */
    void segmentChars(const cv::Mat& binary, std::vector<cv::Mat>& chars,
                      std::vector<cv::Rect>& boxes) const;
#endif
    /** 加载模板集: 字符 → 16x24二值模板 */
    bool loadTemplates(const QString& file);
    /** 模板文件路径 (models/ocr_templates.ini) */
    QString templatePath() const;
    /** 单字符与模板集最优匹配 */
    QPair<QString, double> matchChar(const cv::Mat& norm) const;

    /** 字符模板集 (字符 → 16x24二值图) */
    QMap<QString, cv::Mat> m_templates;

    QList<QRect> m_charRects;   // 叠加缓存
    QString m_lastText;
};
} // namespace VisionInspector
