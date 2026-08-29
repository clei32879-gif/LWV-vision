/**
 * @file Logger.cpp
 * @brief 日志系统实现
 */

#include "Logger.h"
#include "Common.h"

namespace VisionInspector {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (m_logStream) {
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFile) {
        if (m_logFile->isOpen())
            m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }
}

void Logger::init(const QString& logDir) {
    // 确定日志目录
    if (logDir.isEmpty()) {
        // 默认放在 用户文档目录/VisionInspector/Logs
        m_logDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                   + "/VisionInspector/Logs";
    } else {
        m_logDir = logDir;
    }

    // 创建目录
    QDir dir;
    dir.mkpath(m_logDir);

    // 当天日志文件名: VI_2026-06-29.log
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    m_currentLogFile = m_logDir + "/VI_" + today + ".log";

    // 打开文件
    m_logFile = new QFile(m_currentLogFile);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_logStream = new QTextStream(m_logFile);
        m_logStream->setEncoding(QStringConverter::Utf8);

        *m_logStream << "\n========================================\n";
        *m_logStream << "VisionInspector v" << versionString() << "\n";
        *m_logStream << "启动时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
        *m_logStream << "========================================\n\n";
        m_logStream->flush();
    }

    // 清理过期日志
    rotateLogFiles();

    VI_LOG_INFO("日志系统初始化完成");
}

void Logger::log(LogLevel level, const QString& message) {
    // 低于设定级别的不输出
    if (level < m_level)
        return;

    QString prefix = levelToString(level);
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString line = QString("[%1] %2 %3").arg(timestamp, prefix, message);

    // 输出到调试控制台
    switch (level) {
        case LogLevel::Debug: qDebug().noquote() << line; break;
        case LogLevel::Info:  qDebug().noquote() << line; break;
        case LogLevel::Warn:  qWarning().noquote() << line; break;
        case LogLevel::Error: qCritical().noquote() << line; break;
    }

    // 写入文件 (工作线程并发写, 需要互斥)
    QMutexLocker locker(&m_mutex);
    if (m_autoSave && m_logStream) {
        *m_logStream << line << "\n";
        m_logStream->flush();
    }
}

QString Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

void Logger::rotateLogFiles() {
    if (m_maxFileCount <= 0 || !m_autoSave)
        return;

    QDir dir(m_logDir);
    QStringList filters;
    filters << "VI_*.log";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    // 保留最新的 m_maxFileCount 个文件
    while (files.size() > m_maxFileCount) {
        QFileInfo oldest = files.last();
        files.removeLast();
        QFile::remove(oldest.absoluteFilePath());
    }
}

} // namespace VisionInspector
