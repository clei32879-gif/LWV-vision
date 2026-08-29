/** @file JsonHelper.h - JSON辅助工具 */
#pragma once
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QString>
namespace VisionInspector {
class JsonHelper {
public:
    static QJsonObject loadFile(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        auto doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        return doc.object();
    }
    static bool saveFile(const QString& path, const QJsonObject& json) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        f.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
        f.close();
        return true;
    }
};
} // namespace VisionInspector
