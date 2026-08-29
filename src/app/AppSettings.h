/** @file AppSettings.h - 应用全局设置 */
#pragma once
#include "../utils/Common.h"
#include <QObject>
#include <QString>

namespace VisionInspector {

class AppSettings : public QObject {
    Q_OBJECT
public:
    explicit AppSettings(QObject* parent = nullptr) : QObject(parent) {}

    // 软件信息
    static QString appVersion() { return versionString(); }
    static QString appName() { return QStringLiteral("VisionInspector"); }
    static QString appFullName() { return QStringLiteral("VisionInspector 视觉筛选软件"); }
};

} // namespace VisionInspector
