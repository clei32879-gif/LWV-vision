/**
 * @file Logger.h
 * @brief 日志系统 (基于Qt的qDebug, 后续可替换为spdlog)
 *
 * 提供统一的日志接口, 支持分级输出:
 *   VI_LOG_DEBUG(...)  - 调试信息
 *   VI_LOG_INFO(...)   - 一般信息
 *   VI_LOG_WARN(...)   - 警告
 *   VI_LOG_ERROR(...)  - 错误
 *
 * 日志会输出到:
 *   1. 调试输出 (OutputDebugString on Windows)
 *   2. 文件 (后续实现)
 *   3. 日志栏 (LogPanel, 通过信号)
 */

#pragma once

#include <QDebug>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <memory>

namespace VisionInspector {

/**
 * 日志级别
 */
enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3
};

// 支持比较运算
inline bool operator<(LogLevel a, LogLevel b) {
    return static_cast<int>(a) < static_cast<int>(b);
}

/**
 * 日志管理器
 * 单例模式, 管理日志输出和文件存储
 */
class Logger {
public:
    static Logger& instance();

    /**
     * 初始化日志系统
     * @param logDir 日志文件存放目录, 默认在用户文档目录下
     */
    void init(const QString& logDir = QString());

    /**
     * 设置最低日志级别 (低于此级别的不输出)
     */
    void setLevel(LogLevel level) { m_level = level; }

    /**
     * 写日志
     */
    void log(LogLevel level, const QString& message);

    /**
     * 是否启用自动保存到文件
     */
    void setAutoSave(bool enabled) { m_autoSave = enabled; }
    void setMaxFileCount(int count) { m_maxFileCount = count; }

private:
    Logger() = default;
    ~Logger();

    QString levelToString(LogLevel level);
    void rotateLogFiles();

    LogLevel m_level = LogLevel::Debug;
    bool m_autoSave = true;
    int m_maxFileCount = 30;  // 保留30天日志
    QString m_logDir;
    QString m_currentLogFile;
    QFile* m_logFile = nullptr;
    QTextStream* m_logStream = nullptr;
};

} // namespace VisionInspector

// ============================================================
// 日志宏 - 全局使用
// ============================================================

#define VI_LOG_DEBUG(msg)  VisionInspector::Logger::instance().log(VisionInspector::LogLevel::Debug,   msg)
#define VI_LOG_INFO(msg)   VisionInspector::Logger::instance().log(VisionInspector::LogLevel::Info,    msg)
#define VI_LOG_WARN(msg)   VisionInspector::Logger::instance().log(VisionInspector::LogLevel::Warn,    msg)
#define VI_LOG_ERROR(msg)  VisionInspector::Logger::instance().log(VisionInspector::LogLevel::Error,   msg)
