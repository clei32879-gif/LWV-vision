/** @file BarcodeReader.cpp - 条码识别工具实现
 *
 *  引擎选择 (auto 回退):
 *    zxing  — ZXing-C++ (MIT, 支持 Code128/39/EAN/UPC/ITF 等 + DataMatrix + QR, 识别率最优)
 *    opencv — OpenCV5 objdetect BarcodeDetector (捆绑自带)
 *  引擎不可用时自动回退: zxing 请求但库未编入 → opencv。
 */
#include "BarcodeReader.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_ZXING
#include "ReadBarcode.h"
#endif
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
// OpenCV 5.0: 一维条码检测已并入 objdetect 模块 (随库自带, 无需模型/新依赖)
#include <opencv2/objdetect/barcode.hpp>
#endif

namespace VisionInspector {

PropertyDefList BarcodeReader::propertyDefs() const {
    return {
        PropertyDef::enumProp("engine", "识别引擎", {"zxing (推荐)", "opencv"}, 0, "引擎"),
        PropertyDef::boolProp("tryHarder", "深度识别模式(慢)", false, "引擎"),
        PropertyDef::boolProp("useROI", "使用ROI", false),
        PropertyDef::intProp("roiX", "ROI X", 0, 0, 10000),
        PropertyDef::intProp("roiY", "ROI Y", 0, 0, 10000),
        PropertyDef::intProp("roiW", "ROI 宽度", 0, 0, 10000),
        PropertyDef::intProp("roiH", "ROI 高度", 0, 0, 10000),
    };
}

#ifdef VI_HAS_ZXING
static QString zxingFormatName(ZXing::BarcodeFormat f) {
    using F = ZXing::BarcodeFormat;
    if (f == F::EAN13) return "EAN13";
    if (f == F::EAN8) return "EAN8";
    if (f == F::UPCA) return "UPC-A";
    if (f == F::UPCE) return "UPC-E";
    if (f == F::Code128) return "Code128";
    if (f == F::Code39) return "Code39";
    if (f == F::Code93) return "Code93";
    if (f == F::ITF) return "ITF";
    if (f == F::Codabar) return "Codabar";
    if (f == F::DataMatrix) return "DataMatrix";
    if (f == F::QRCode) return "QRCode";
    if (f == F::PDF417) return "PDF417";
    if (f == F::Aztec) return "Aztec";
    return QString::fromUtf8(ZXing::ToString(f));
}
#endif

bool BarcodeReader::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    m_lastFound = false;
    m_lastCorners.clear();

    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    cv::Mat gray;
    if (input->channels() > 1) cv::cvtColor(*input, gray, cv::COLOR_BGR2GRAY);
    else gray = *input; // 只读, 浅拷贝即可

    // ROI 裁剪 (可选)
    cv::Mat roi = gray;
    double offX = 0, offY = 0;
    if (propertyValue("useROI").toBool()) {
        const cv::Rect r = cv::Rect(propertyValue("roiX").toInt(), propertyValue("roiY").toInt(),
                                    propertyValue("roiW").toInt(), propertyValue("roiH").toInt())
                           & cv::Rect(0, 0, gray.cols, gray.rows);
        if (r.width >= 8 && r.height >= 8) {
            roi = gray(r);
            offX = r.x; offY = r.y;
        }
    }

    setResultData("barcodeDetected", false);

#ifdef VI_HAS_ZXING
    // ---- ZXing 引擎 (默认) ----
    if (propertyValue("engine").toInt() == 0) {
        try {
            using namespace ZXing;
            ReaderOptions hints;
            hints.setTryHarder(propertyValue("tryHarder").toBool());
            hints.setTryRotate(true);
            hints.setFormats(BarcodeFormat::Any);
            const auto img = ImageView(roi.data, roi.cols, roi.rows, ImageFormat::Lum);
            const auto results = ReadBarcodes(img, hints);
            if (!results.empty()) {
                m_lastFound = true;
                setResultData("barcodeDetected", true);
                setResultData("barcodeCount", (int)results.size());
                setResultData("barcodeText", QString::fromStdString(results[0].text()));
                setResultData("barcodeType", zxingFormatName(results[0].format()));
                QVariantList quads;
                double cx = 0, cy = 0;
                for (size_t i = 0; i < results.size(); ++i) {
                    const auto& pos = results[i].position();
                    const QPointF pts[4] = {
                        QPointF(pos.topLeft().x + offX, pos.topLeft().y + offY),
                        QPointF(pos.topRight().x + offX, pos.topRight().y + offY),
                        QPointF(pos.bottomRight().x + offX, pos.bottomRight().y + offY),
                        QPointF(pos.bottomLeft().x + offX, pos.bottomLeft().y + offY) };
                    QVariantList quad;
                    for (int j = 0; j < 4; ++j) quad.append(QVariantList{ pts[j].x(), pts[j].y() });
                    quads.append(quad);
                    if (i == 0) {
                        for (int j = 0; j < 4; ++j) {
                            m_lastCorners.append(pts[j]);
                            cx += pts[j].x(); cy += pts[j].y();
                        }
                    }
                }
                setResultData("boxQuads", quads);
                setResultData("centerX", cx / 4.0);
                setResultData("centerY", cy / 4.0);
                setStatus(ToolStatus::OK);
                return true;
            }
            // zxing 没找到 → 不再回退 opencv (zxing 识别率更高, 找不到基本是真没有)
            setResultData("barcodeText", "");
            setStatus(ToolStatus::NG);
            return false;
        } catch (const std::exception& e) {
            setResultData("error", QString::fromUtf8(e.what()));
            setStatus(ToolStatus::NG);
            return false;
        }
    }
#endif

    // ---- OpenCV 引擎 (回退/可选) ----
    try {
        cv::barcode::BarcodeDetector detector;
        std::vector<std::string> infos, types;
        cv::Mat corners; // 每4个CV_32FC2点为一个条码四角
        const bool found = detector.detectAndDecodeWithType(roi, infos, types, corners);
        if (found && !infos.empty()) {
            m_lastFound = true;
            setResultData("barcodeDetected", true);
            setResultData("barcodeCount", (int)infos.size());
            setResultData("barcodeText", QString::fromStdString(infos[0]));
            setResultData("barcodeType", types.empty()
                                             ? QStringLiteral("unknown")
                                             : QString::fromStdString(types[0]));
            if (!corners.empty() && corners.total() >= 4) {
                const int n = (int)(corners.total() / 4);
                QVariantList quads;
                double cx = 0, cy = 0;
                for (int i = 0; i < n; ++i) {
                    QVariantList quad;
                    for (int j = 0; j < 4; ++j) {
                        const cv::Point2f p = corners.at<cv::Point2f>(i * 4 + j);
                        quad.append(QVariantList{ p.x + offX, p.y + offY });
                        if (i == 0) {
                            const QPointF q(p.x + offX, p.y + offY);
                            m_lastCorners.append(q);
                            cx += q.x(); cy += q.y();
                        }
                    }
                    quads.append(quad);
                }
                setResultData("boxQuads", quads);
                setResultData("centerX", cx / 4.0);
                setResultData("centerY", cy / 4.0);
            }
            setStatus(ToolStatus::OK);
            return true;
        }
    } catch (const cv::Exception& e) {
        setResultData("error", QString::fromUtf8(e.what()));
        setStatus(ToolStatus::NG);
        return false;
    }
    setResultData("barcodeText", "");
    setStatus(ToolStatus::NG);
    return false;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

std::vector<QVariant> BarcodeReader::overlays() const {
    std::vector<QVariant> out;
    if (!m_lastFound || m_lastCorners.size() < 4) return out;
    // 条码四角框 (绿色)
    QVariantMap box;
    box["type"] = "lines";
    QVariantList segs;
    for (int j = 0; j < 4; ++j) {
        const QPointF& a = m_lastCorners[j];
        const QPointF& b = m_lastCorners[(j + 1) % 4];
        segs.append(QVariantList{ a.x(), a.y(), b.x(), b.y() });
    }
    box["lines"] = segs;
    box["color"] = "#00ff00";
    out.push_back(box);
    return out;
}

VI_REGISTER_TOOL(BarcodeReader, "条码识别", VisionInspector::ToolCategory::Detection)
} // namespace VisionInspector
