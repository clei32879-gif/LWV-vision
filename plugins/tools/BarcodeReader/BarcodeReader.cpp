#include "BarcodeReader.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
// OpenCV 5.0: 一维条码检测已并入 objdetect 模块 (随库自带, 无需模型/新依赖)
#include <opencv2/objdetect/barcode.hpp>
#endif

namespace VisionInspector {

PropertyDefList BarcodeReader::propertyDefs() const {
    return {
        PropertyDef::boolProp("useROI", "使用ROI", false),
        PropertyDef::intProp("roiX", "ROI X", 0, 0, 10000),
        PropertyDef::intProp("roiY", "ROI Y", 0, 0, 10000),
        PropertyDef::intProp("roiW", "ROI 宽度", 0, 0, 10000),
        PropertyDef::intProp("roiH", "ROI 高度", 0, 0, 10000),
    };
}

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
