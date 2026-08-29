/** @file Utils.h - 通用工具函数 */
#pragma once
#include <QString>
#include <QStandardPaths>
#include <QDir>
namespace VisionInspector {
class Utils {
public:
    static QString appDataDir() {
        auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        return dir;
    }
    static QString formatDouble(double val, int precision = 3) {
        return QString::number(val, 'f', precision);
    }
};
} // namespace VisionInspector
