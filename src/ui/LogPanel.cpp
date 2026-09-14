#include "LogPanel.h"
#include <QVBoxLayout>
#include <QDateTime>

namespace VisionInspector {

LogPanel::LogPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setMaximumBlockCount(1000); // 最多保留1000行
    m_textEdit->setStyleSheet("background-color: #e6eff9; color: #3a5a7c; font-family: monospace;");
    layout->addWidget(m_textEdit);
}

void LogPanel::appendLog(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_textEdit->appendPlainText(QString("[%1] %2").arg(timestamp, message));
}

void LogPanel::clearLog() {
    m_textEdit->clear();
}

} // namespace VisionInspector
