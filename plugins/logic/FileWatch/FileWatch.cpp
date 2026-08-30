/**
 * @file FileWatch.cpp
 * @brief 文件监测工具 (对标 CKVision 文件监测, P0-10 补齐)
 *
 * 现场低成本握手: 上位机/PLC 通过落一个"信号文件"与视觉机对接。
 * - 模式 检查存在: 返回 fileExists 布尔 (流程可据此分支/循环等待)
 * - 模式 删除文件: 删除指定文件, 输出 deleted
 * - 模式 清理旧文件: 删除目录下超过保留天数的文件
 *
 * 输出: fileExists / deleted / cleanedCount
 */
#include "FileWatch.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QFileInfoList>

namespace VisionInspector {

PropertyDefList FileWatch::propertyDefs() const {
    return {
        PropertyDef::enumProp("watchMode", "模式", {"检查存在", "删除文件", "清理旧文件"}, 0),
        PropertyDef::stringProp("filePath", "文件全路径", ""),
        PropertyDef::intProp("keepDays", "保留天数(清理模式)", 7, 0, 3650),
    };
}

bool FileWatch::execute(ToolContext& context) {
    const int mode = propertyValue("watchMode").toInt();
    const QString path = propertyValue("filePath").toString();
    const int keepDays = propertyValue("keepDays").toInt();

    setResultData("filePath", path);

    if (mode == 0) {   // 检查存在
        const bool exists = !path.isEmpty() && QFileInfo::exists(path);
        setResultData("fileExists", exists);
        setResultData("deleted", false);
        setResultData("cleanedCount", 0);
        setStatus(ToolStatus::OK);
        return true;
    }

    if (mode == 1) {   // 删除文件
        bool deleted = false;
        QString err;
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            deleted = QFile::remove(path);
            if (!deleted) err = QStringLiteral("删除失败");
        }
        setResultData("fileExists", QFileInfo::exists(path));
        setResultData("deleted", deleted);
        setResultData("error", err);
        setResultData("cleanedCount", 0);
        setStatus(deleted ? ToolStatus::OK : ToolStatus::NG);
        return true;
    }

    // 清理旧文件: 目录下超过 keepDays 天的文件
    int cleaned = 0;
    QDir dir(path);
    if (dir.exists()) {
        const qint64 cutoff = QDateTime::currentDateTime()
                                  .addDays(-keepDays).toMSecsSinceEpoch();
        const QFileInfoList entries = dir.entryInfoList(QDir::Files);
        for (const QFileInfo& fi : entries) {
            if (fi.lastModified().toMSecsSinceEpoch() < cutoff) {
                if (QFile::remove(fi.absoluteFilePath())) ++cleaned;
            }
        }
    }
    setResultData("fileExists", false);
    setResultData("deleted", false);
    setResultData("cleanedCount", cleaned);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(FileWatch, "文件监测", VisionInspector::ToolCategory::Logic)
