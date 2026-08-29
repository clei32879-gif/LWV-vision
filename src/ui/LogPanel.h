/** @file LogPanel.h - 日志栏 */
#pragma once
#include <QWidget>
#include <QPlainTextEdit>

namespace VisionInspector {

class LogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);
    void appendLog(const QString& message);
    void clearLog();

private:
    QPlainTextEdit* m_textEdit;
};

} // namespace VisionInspector
