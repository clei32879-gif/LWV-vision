/**
 * @file WriteText.cpp
 * @brief 写入文本工具 (对标 CKVision 写入文本, P0-10 补齐)
 *
 * 将流程中的文本/结果数据保存到指定文件夹下的文本文件。
 * - content 支持 $(工具名.键) 引用 (引擎执行前自动解析)
 * - append=false 时覆盖文件(仅保留最新一次执行结果), =true 时追加
 * - 输出: filePath / written(bool) / content
 */
#include "WriteText.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

namespace VisionInspector {

PropertyDefList WriteText::propertyDefs() const {
    return {
        PropertyDef::stringProp("directory", "保存目录", "Data"),
        PropertyDef::stringProp("fileName", "文件名(不含扩展名)", "Data"),
        PropertyDef::boolProp("addDateSuffix", "文件名加日期后缀", false),
        PropertyDef::boolProp("addTimeSuffix", "文件名加时分秒后缀", false),
        PropertyDef::stringProp("extension", "扩展名", "csv"),
        PropertyDef::stringProp("content", "写入内容", ""),
        PropertyDef::boolProp("append", "追加模式(否则覆盖)", true),
    };
}

bool WriteText::execute(ToolContext& context) {
    QString dir  = propertyValue("directory").toString();
    QString name = propertyValue("fileName").toString();
    QString ext  = propertyValue("extension").toString();
    if (ext.isEmpty()) ext = QStringLiteral("txt");
    if (ext.startsWith(QLatin1Char('.'))) ext = ext.mid(1);
    if (name.isEmpty()) name = QStringLiteral("Data");

    const bool addDate = propertyValue("addDateSuffix").toBool();
    const bool addTime = propertyValue("addTimeSuffix").toBool();
    const QString content = propertyValue("content").toString();
    const bool append = propertyValue("append").toBool();

    // 文件名后缀
    QString suffix;
    if (addDate) suffix += QDateTime::currentDateTime().toString("yyyyMMdd");
    if (addTime) suffix += QDateTime::currentDateTime().toString("HHmmss");
    const QString fileName = name + suffix + QLatin1Char('.') + ext;

    QDir qdir(dir);
    if (!qdir.exists() && !qdir.mkpath(QStringLiteral("."))) {
        setResultData("filePath", QString());
        setResultData("written", false);
        setResultData("error", "目录创建失败: " + dir);
        setStatus(ToolStatus::NG);
        return true;
    }
    const QString filePath = QDir(dir).filePath(fileName);

    QFile file(filePath);
    const QIODevice::OpenMode mode = append ? (QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)
                                            : (QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    if (!file.open(mode)) {
        setResultData("filePath", filePath);
        setResultData("written", false);
        setResultData("error", "文件打开失败: " + file.errorString());
        setStatus(ToolStatus::NG);
        return true;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    if (!content.endsWith(QLatin1Char('\n')))
        out << Qt::endl;
    out.flush();
    file.close();

    setResultData("filePath", filePath);
    setResultData("written", true);
    setResultData("content", content);
    setResultData("append", append);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(WriteText, "写入文本", VisionInspector::ToolCategory::Logic)
