/**
 * @file OCR.cpp
 * @brief 字符识别工具 — 真实实现 (字符分割 + 归一化模板匹配, 无外部依赖)
 *
 * 工作流 (对齐CKVision"字符集合/字符读取"):
 *   模式0 教导: 用户输入该ROI处的期望文本 → 分割出的字符图逐一归一化16x24
 *               → 与期望文本字符一一对应 → 模板存 models/ocr_templates.ini
 *   模式1 识别: 分割 → 归一化 → 与全部模板算相关系数 → 最优匹配输出文本
 *
 * 适配: 固定印刷体/喷码 (产线日期批号/丝印字符), 每个新字符只需教导一次.
 */
#include "OCR.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#endif
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <climits>
#include <cmath>
#include <QVariantMap>

namespace VisionInspector {

namespace {
constexpr int kNormW = 16, kNormH = 24;

/** 字符归一化: 保持纵横比缩放到16x24画布居中 (连通域/投影两条分割路径共用) */
void normalizeChar(const cv::Mat& ch, cv::Mat& norm) {
    norm = cv::Mat(kNormH, kNormW, CV_8UC1, cv::Scalar(0));
    const double sc = std::min((double)kNormW / ch.cols, (double)kNormH / ch.rows);
    cv::Mat scaled;
    cv::resize(ch, scaled, cv::Size(), sc, sc, cv::INTER_NEAREST);
    const int ox = (kNormW - scaled.cols) / 2, oy = (kNormH - scaled.rows) / 2;
    scaled.copyTo(norm(cv::Rect(ox, oy, scaled.cols, scaled.rows)));
}
}

QString OCR::templatePath() const {
    // 模板全局共享(同一台机器的字符样式通常一致), 放exe旁 models/
    return QCoreApplication::applicationDirPath() + "/models/ocr_templates.ini";
}

PropertyDefList OCR::propertyDefs() const {
    return {
        PropertyDef::enumProp("mode", "工作模式", {"识别", "教导"}, 0, "模式"),
        PropertyDef::stringProp("teachText", "教导文本(模式1)", "", "模式"),
        PropertyDef::boolProp("useROI", "使用ROI", true, "ROI"),
        PropertyDef::intProp("roiX", "ROI X", 100, 0, 10000, "ROI"),
        PropertyDef::intProp("roiY", "ROI Y", 100, 0, 10000, "ROI"),
        PropertyDef::intProp("roiW", "ROI 宽度", 300, 1, 10000, "ROI"),
        PropertyDef::intProp("roiH", "ROI 高度", 80, 1, 10000, "ROI"),
        PropertyDef::enumProp("charType", "字符类型", {"全部", "数字", "字母", "数字+字母"}, 3, "识别"),
        PropertyDef::intProp("thresholdValue", "二值化阈值(0=Otsu自动)", 0, 0, 255, "识别"),
        PropertyDef::boolProp("invert", "亮字暗底(白字黑底勾选)", true, "识别"),
        PropertyDef::doubleProp("matchThreshold", "匹配置信度下限", 0.55, 0.1, 1.0, "识别"),
        PropertyDef::enumProp("segmentMethod", "分割方法", {"连通域", "垂直投影(粘连字符)"}, 0, "分割"),
        PropertyDef::intProp("minCharHeight", "最小字符高度(px)", 12, 2, 500, "分割"),
        PropertyDef::intProp("maxCharHeight", "最大字符高度(px)", 200, 2, 2000, "分割"),
        PropertyDef::intProp("charGap", "字符间距容差(px)", 20, 1, 200, "分割"),
    };
}

#ifdef VI_HAS_OPENCV
void OCR::segmentChars(const cv::Mat& binary, std::vector<cv::Mat>& chars,
                       std::vector<cv::Rect>& boxes) const {
    const int minH = propertyValue("minCharHeight").toInt();
    const int maxH = propertyValue("maxCharHeight").toInt();

    // 连通域分割
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<cv::Rect> cand;
    for (const auto& c : contours) {
        cv::Rect r = cv::boundingRect(c);
        // 主体字符高度过滤; 短横杠类(-_~=)高度很小但宽度大, 单独保留
        const bool dashLike = (r.width >= r.height * 3 && r.width >= 6);
        if (!dashLike && (r.height < minH || r.height > maxH)) continue;
        if (r.width < 2) continue;
        // 面积占比过小为噪声
        if (r.area() < r.height) continue;   // 外接矩形太扁=噪声(注: 线状字符'-'靠外接矩形保留)
        cand.push_back(r);
    }
    if (cand.empty()) return;

    // x排序 (阅读顺序); 宽度接近的相邻框合并 (字符断裂/汉字多部件)
    std::sort(cand.begin(), cand.end(),
              [](const cv::Rect& a, const cv::Rect& b) { return a.x < b.x; });
    const int gap = propertyValue("charGap").toInt();
    std::vector<cv::Rect> merged;
    for (const cv::Rect& r : cand) {
        if (!merged.empty()) {
            cv::Rect& last = merged.back();
            // 同字符部件合并条件 (极严): 水平重叠/相触(断裂部件几乎共边)
            // 且垂直重叠>60%. 字符间正常空隙>=1px时不合并 — 分隔交给
            // 高度过滤与教导对齐 (产线字体字符间距通常>=1px可分).
            const int hGap = r.x - (last.x + last.width);
            const int vOverlap = std::min(r.y + r.height, last.y + last.height)
                               - std::max(r.y, last.y);
            const int vMin = std::min(r.height, last.height);
            if (hGap <= 0 && vOverlap * 10 >= vMin * 6) {
                last |= r;
                continue;
            }
        }
        merged.push_back(r);
    }

    // 逐字符裁剪归一化 16x24
    for (const cv::Rect& r : merged) {
        cv::Mat ch = binary(r);
        cv::Mat norm;
        normalizeChar(ch, norm);
        chars.push_back(norm);
        boxes.push_back(r);
    }
}

/** #10 投影分割备选: 垂直投影峰谷切分
 *  适用场景: 字符相互粘连/接触, 连通域法把它们当成一个大块分不开。
 *  算法: 行带定位(水平投影最大行带) → 带内垂直投影 → 零谷切分 + 小间隙合并
 *        → 超宽块按估计字宽在投影局部极小处二次切分(处理部分粘连) */
void OCR::segmentCharsByProjection(const cv::Mat& binary, std::vector<cv::Mat>& chars,
                                   std::vector<cv::Rect>& boxes) const {
    const int minH = propertyValue("minCharHeight").toInt();
    const int maxH = propertyValue("maxCharHeight").toInt();
    const int gap = propertyValue("charGap").toInt();
    if (binary.empty()) return;

    // ---- 行带定位: 水平投影取最大连续行带 (单行OCR场景) ----
    cv::Mat rowProj;
    cv::reduce(binary, rowProj, 1, cv::REDUCE_SUM, CV_32S);
    int rowMax = 0;
    for (int r = 0; r < rowProj.rows; ++r) rowMax = std::max(rowMax, rowProj.at<int>(r, 0));
    if (rowMax <= 0) return;
    int bandY0 = -1, bandY1 = -1, best0 = 0, best1 = 0, bestSum = -1, run0 = -1;
    long long runSum = 0;
    for (int r = 0; r <= rowProj.rows; ++r) {
        const bool fg = (r < rowProj.rows) && rowProj.at<int>(r, 0) >= rowMax / 10;
        if (fg && run0 < 0) { run0 = r; runSum = 0; }
        if (fg) runSum += rowProj.at<int>(r, 0);
        if ((!fg || r == rowProj.rows) && run0 >= 0) {
            if ((long long)runSum > bestSum) {
                bestSum = runSum; best0 = run0; best1 = r;
            }
            run0 = -1;
        }
    }
    bandY0 = best0; bandY1 = best1;
    if (bandY1 - bandY0 < 2) return;

    const cv::Mat band = binary(cv::Rect(0, bandY0, binary.cols, bandY1 - bandY0));

    // ---- 垂直投影 ----
    cv::Mat colProjM;
    cv::reduce(band, colProjM, 0, cv::REDUCE_SUM, CV_32S);
    std::vector<int> proj(colProjM.cols);
    int projMax = 0;
    for (int x = 0; x < colProjM.cols; ++x) {
        proj[x] = colProjM.at<int>(0, x);
        projMax = std::max(projMax, proj[x]);
    }
    if (projMax <= 0) return;

    // 平滑(宽度3)去噪
    std::vector<int> sm(proj.size(), 0);
    for (int x = 0; x < (int)proj.size(); ++x) {
        int lo = std::max(0, x - 1), hi = std::min((int)proj.size() - 1, x + 1);
        sm[x] = (proj[lo] + proj[x] + proj[hi]) / 3;
    }

    // 前景列段 (阈值=最大投影5%)
    const int bgLevel = std::max(1, projMax / 20);
    std::vector<cv::Rect> cand;
    int runStart = -1;
    for (int x = 0; x <= (int)sm.size(); ++x) {
        const bool fg = (x < (int)sm.size()) && sm[x] > bgLevel;
        if (fg && runStart < 0) runStart = x;
        if ((!fg || x == (int)sm.size()) && runStart >= 0) {
            cand.push_back(cv::Rect(runStart, bandY0, x - runStart, bandY1 - bandY0));
            runStart = -1;
        }
    }
    if (cand.empty()) return;

    // 小间隙合并 (断裂部件: 间隙<=charGap)
    std::vector<cv::Rect> merged;
    for (const cv::Rect& r : cand) {
        if (!merged.empty()) {
            cv::Rect& last = merged.back();
            if (r.x - (last.x + last.width) <= gap) { last |= r; continue; }
        }
        merged.push_back(r);
    }

    // 超宽块二次切分: 宽度>1.8倍估计字宽(0.6*高度)时, 在块内投影局部极小处切
    // 切分数 = round(宽 / 估计字宽)
    std::vector<cv::Rect> finalBoxes;
    for (const cv::Rect& r : merged) {
        const double estW = 0.6 * r.height;
        const int nSplit = std::max(1, (int)std::lround(r.width / std::max(4.0, estW * 1.6)));
        if (nSplit <= 1) { finalBoxes.push_back(r); continue; }
        // 在块内找 nSplit-1 个最低投影列作为切点 (等距邻域内搜索)
        std::vector<int> cuts;
        const double segW = (double)r.width / nSplit;
        for (int k = 1; k < nSplit; ++k) {
            const int cx0 = r.x + (int)(segW * (k - 0.45));
            const int cx1 = r.x + (int)(segW * (k + 0.45));
            int bestX = r.x + (int)(segW * k);
            int bestV = INT_MAX;
            for (int x = std::max(r.x, cx0); x <= std::min(r.x + r.width - 1, cx1); ++x) {
                if (x >= 0 && x < (int)proj.size() && proj[x] < bestV) { bestV = proj[x]; bestX = x; }
            }
            cuts.push_back(bestX);
        }
        int prev = r.x;
        for (int cut : cuts) {
            finalBoxes.push_back(cv::Rect(prev, r.y, cut - prev, r.height));
            prev = cut;
        }
        finalBoxes.push_back(cv::Rect(prev, r.y, r.x + r.width - prev, r.height));
    }

    // 高度过滤 + 归一化 (与连通域路径同一套判定)
    for (const cv::Rect& r : finalBoxes) {
        if (r.width < 2) continue;
        const bool dashLike = (r.width >= r.height * 3 && r.width >= 6);
        if (!dashLike && (r.height < minH || r.height > maxH)) continue;
        cv::Mat ch = band(cv::Rect(r.x, 0, r.width, band.rows));
        cv::Mat norm;
        normalizeChar(ch, norm);
        chars.push_back(norm);
        boxes.push_back(r);
    }
}
#endif

bool OCR::loadTemplates(const QString& file) {
#ifdef VI_HAS_OPENCV
    m_templates.clear();
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        const QStringList parts = line.split(',');
        if (parts.size() < 2) continue;
        const QString ch = parts[0];
        cv::Mat norm(kNormH, kNormW, CV_8UC1, cv::Scalar(0));
        if (parts[1].length() != kNormW * kNormH) continue;
        for (int i = 0; i < kNormW * kNormH; ++i)
            norm.at<uchar>(i / kNormW, i % kNormW) = (parts[1][i] == '1') ? 255 : 0;
        m_templates[ch] = norm;
    }
    return !m_templates.isEmpty();
#else
    return false;
#endif
}

QPair<QString, double> OCR::matchChar(const cv::Mat& norm) const {
    QString best; double bestScore = -1;
    // 匹配分数 = 归一化相关 (前景重叠度), 模板与输入均为0/255二值
    for (auto it = m_templates.begin(); it != m_templates.end(); ++it) {
        cv::Mat mul;
        cv::multiply(norm, it.value(), mul);
        const float inter = cv::countNonZero(mul);
        const float uni = cv::countNonZero(norm) + cv::countNonZero(it.value()) - inter;
        const double score = uni > 0 ? inter / uni : 0;   // Jaccard相似度
        if (score > bestScore) { bestScore = score; best = it.key(); }
    }
    return {best, bestScore};
}

bool OCR::execute(ToolContext& context) {
#ifndef VI_HAS_OPENCV
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#else
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        setResultData("error", "输入图像为空"); setStatus(ToolStatus::NG); return false;
    }
    cv::Mat src;
    if (input->channels() > 1) cv::cvtColor(*input, src, cv::COLOR_BGR2GRAY);
    else src = *input;

    // ROI
    cv::Mat roi = src;
    if (propertyValue("useROI").toBool()) {
        const int x = propertyValue("roiX").toInt(), y = propertyValue("roiY").toInt();
        const int w = propertyValue("roiW").toInt(), h = propertyValue("roiH").toInt();
        if (w > 0 && h > 0 && x < src.cols && y < src.rows)
            roi = src(cv::Rect(x, y, std::min(w, src.cols - x), std::min(h, src.rows - y)));
    }
    // ROI也做补正跟随 (模板坐标 → 图像坐标)
    // 注: useCorrection开启时 roiX/Y 是教导坐标, 由引擎级applyCorrection思路变换
    // 简化: OCR的ROI跟随交给用户在补正流程里用CropTransform先行裁剪; ROI自身保持图像坐标

    // 二值化 (0=Otsu自动)
    cv::Mat binary;
    const int threshVal = propertyValue("thresholdValue").toInt();
    // invert=白字黑底(亮字暗底)→THRESH_BINARY提取亮字; 黑字白底→INV提取暗字
    const int flag = propertyValue("invert").toBool() ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;
    if (threshVal <= 0)
        cv::threshold(roi, binary, 0, 255, flag | cv::THRESH_OTSU);
    else
        cv::threshold(roi, binary, threshVal, 255, flag);

    // 分割 (#10: 连通域默认 / 垂直投影备选解决粘连字符)
    std::vector<cv::Mat> chars;
    std::vector<cv::Rect> boxes;
    if (propertyValue("segmentMethod").toInt() == 1)
        segmentCharsByProjection(binary, chars, boxes);
    else
        segmentChars(binary, chars, boxes);
    m_charRects.clear();
    for (const cv::Rect& r : boxes)
        m_charRects.append(QRect(r.x + (roi.data != src.data ? propertyValue("roiX").toInt() : 0),
                                 r.y + (roi.data != src.data ? propertyValue("roiY").toInt() : 0),
                                 r.width, r.height));
    setResultData("charCount", (int)chars.size());
    if (chars.empty()) {
        setResultData("error", "未分割到字符 (检查阈值/极性/字符高度范围)");
        setStatus(ToolStatus::NG);
        return false;
    }

    // ---- 模式1: 教导 ----
    const int mode = propertyValue("mode").toInt();
    if (mode == 1) {
        const QString teachText = propertyValue("teachText").toString().trimmed();
        if (teachText.isEmpty()) {
            setResultData("error", "教导模式需填写教导文本 (与图像字符一一对应)");
            setStatus(ToolStatus::NG);
            return false;
        }
        // 去掉文本里的空格后逐字符对应
        QString expect;
        for (const QChar& c : teachText)
            if (!c.isSpace()) expect.append(c);
        if (expect.length() != (int)chars.size()) {
            setResultData("error", QString("教导失败: 图像分割出%1个字符, 教导文本%2个字符 — 数量须一致")
                              .arg(chars.size()).arg(expect.length()));
            setStatus(ToolStatus::NG);
            return false;
        }
        // 合并保存模板 (读旧+加新+写回)
        QMap<QString, cv::Mat> all;
        QFile rf(templatePath());
        if (rf.open(QIODevice::ReadOnly)) {
            QTextStream in(&rf);
            while (!in.atEnd()) {
                const QString line = in.readLine().trimmed();
                const int comma = line.indexOf(',');
                if (comma <= 0) continue;
                const QString ch = line.left(comma);
                const QString data = line.mid(comma + 1);
                cv::Mat norm(kNormH, kNormW, CV_8UC1, cv::Scalar(0));
                if (data.length() == kNormW * kNormH)
                    for (int i = 0; i < kNormW * kNormH; ++i)
                        norm.at<uchar>(i / kNormW, i % kNormW) = (data[i] == '1') ? 255 : 0;
                all[ch] = norm;
            }
        }
        for (size_t i = 0; i < chars.size(); ++i)
            all[QString(expect[int(i)])] = chars[i];

        QDir(QFileInfo(templatePath()).absolutePath()).mkpath(".");
        QFile wf(templatePath());
        if (!wf.open(QIODevice::WriteOnly)) {
            setResultData("error", "模板文件写入失败: " + templatePath());
            setStatus(ToolStatus::NG);
            return false;
        }
        QTextStream out(&wf);
        for (auto it = all.begin(); it != all.end(); ++it) {
            QString data;
            for (int r = 0; r < kNormH; ++r)
                for (int c = 0; c < kNormW; ++c)
                    data += (it.value().at<uchar>(r, c) ? '1' : '0');
            out << it.key() << ',' << data << '\n';
        }
        m_lastText = QStringLiteral("教导%1个字符模板").arg(chars.size());
        setResultData("recognizedText", m_lastText);
        setResultData("taughtCount", (int)chars.size());
        setStatus(ToolStatus::OK);
        return true;
    }

    // ---- 模式0: 识别 ----
    if (!loadTemplates(templatePath())) {
        setResultData("error", "无字符模板 — 请先用教导模式学习");
        setStatus(ToolStatus::NG);
        return false;
    }

    const double matchThr = propertyValue("matchThreshold").toDouble();
    QString text;
    double minScore = 1.0;
    for (const cv::Mat& ch : chars) {
        const auto m = matchChar(ch);
        if (m.second < matchThr) {
            text += '?';
            minScore = std::min(minScore, m.second);
        } else {
            text += m.first;
            minScore = std::min(minScore, m.second);
        }
    }
    m_lastText = text;
    setResultData("recognizedText", text);
    setResultData("minMatchScore", minScore);
    setStatus(ToolStatus::OK);
    return true;
#endif
}

std::vector<QVariant> OCR::overlays() const {
    std::vector<QVariant> out;
    for (const QRect& r : m_charRects) {
        QVariantMap rect;
        rect["type"] = "rect";
        rect["x"] = r.x(); rect["y"] = r.y();
        rect["w"] = r.width(); rect["h"] = r.height();
        rect["color"] = "#00ccff";
        out.push_back(rect);
    }
    if (!m_lastText.isEmpty()) {
        QVariantMap text;
        text["type"] = "text";
        text["x"] = 10.0; text["y"] = 22.0; text["size"] = 16.0;
        text["text"] = QStringLiteral("OCR: %1").arg(m_lastText);
        text["color"] = "#ffff00";
        out.push_back(text);
    }
    return out;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(OCR, "字符识别", VisionInspector::ToolCategory::Detection)
