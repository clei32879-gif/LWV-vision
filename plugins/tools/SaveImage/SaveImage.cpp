/**
 * @file SaveImage.cpp
 * @brief 存储图像工具 (对标 CKVision 存储图像, P1-11 补齐)
 *
 * 将当前图像 (Current, 或 inputImage 属性指定的命名图像) 保存到指定目录。
 * - 目录不存在自动创建
 * - 文件名可加 日期/时分秒 后缀; 扩展名 png/jpg/bmp (jpg 可设质量)
 * - 输出: filePath / saved(bool)
 */
#include "SaveImage.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#endif
#include <QDir>
#include <QDateTime>

namespace VisionInspector {

PropertyDefList SaveImage::propertyDefs() const {
    return {
        PropertyDef::stringProp("directory", "存储目录", "SavedImages"),
        PropertyDef::stringProp("fileName", "文件名(不含扩展名)", "Image"),
        PropertyDef::boolProp("addDateSuffix", "文件名加日期后缀", false),
        PropertyDef::boolProp("addTimeSuffix", "文件名加时分秒后缀", false),
        PropertyDef::enumProp("format", "格式", {"PNG", "JPG", "BMP"}, 0),
        PropertyDef::intProp("jpegQuality", "JPG质量", 95, 1, 100),
    };
}

bool SaveImage::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }

    QString dir  = propertyValue("directory").toString();
    QString name = propertyValue("fileName").toString();
    const int fmt = propertyValue("format").toInt();
    const QString ext = fmt == 0 ? "png" : (fmt == 1 ? "jpg" : "bmp");
    if (name.isEmpty()) name = QStringLiteral("Image");

    QString suffix;
    if (propertyValue("addDateSuffix").toBool())
        suffix += QDateTime::currentDateTime().toString("yyyyMMdd");
    if (propertyValue("addTimeSuffix").toBool())
        suffix += QDateTime::currentDateTime().toString("HHmmss");
    const QString fileName = name + suffix + QLatin1Char('.') + ext;

    QDir qdir(dir);
    if (!qdir.exists() && !qdir.mkpath(QStringLiteral("."))) {
        setResultData("filePath", QString());
        setResultData("saved", false);
        setResultData("error", "目录创建失败: " + dir);
        setStatus(ToolStatus::NG);
        return true;
    }
    const QString filePath = QDir(dir).filePath(fileName);

    std::vector<int> params;
    if (fmt == 1) {
        params.push_back(cv::IMWRITE_JPEG_QUALITY);
        params.push_back(propertyValue("jpegQuality").toInt());
    }
    const bool ok = cv::imwrite(filePath.toLocal8Bit().toStdString(), *input, params);

    setResultData("filePath", filePath);
    setResultData("saved", ok);
    setStatus(ok ? ToolStatus::OK : ToolStatus::NG);
    return ok;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(SaveImage, "存储图像", VisionInspector::ToolCategory::ImageProcess)
