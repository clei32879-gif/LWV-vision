/**
 * @file SettingsDialogs.h
 * @brief 系统设置 / 项目设置 / 全局变量 三个对话框
 */
#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QCheckBox>

namespace VisionInspector {

class GlobalVariables;

// ============================================================
// 系统设置: 日志级别 / 图像保存目录 / 自动保存
// ============================================================
class SystemSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SystemSettingsDialog(QWidget* parent = nullptr);

private slots:
    void onAccept();

private:
    QComboBox* m_logLevel = nullptr;
    QLineEdit* m_imageDir = nullptr;
    QComboBox* m_language = nullptr;
    QCheckBox* m_autoSaveNG = nullptr;
};

// ============================================================
// 项目设置: 项目名 / 备注 / 作者
// ============================================================
class ProjectSettingsDialog : public QDialog {
    Q_OBJECT
public:
    ProjectSettingsDialog(const QString& name, const QString& note,
                          QWidget* parent = nullptr);
    QString projectName() const { return m_name->text(); }
    QString projectNote() const { return m_note->toPlainText(); }

private:
    QLineEdit* m_name = nullptr;
    QPlainTextEdit* m_note = nullptr;
};

// ============================================================
// 全局变量管理: 增/删/改变量 (跨流程共享)
// ============================================================
class GlobalVariablesDialog : public QDialog {
    Q_OBJECT
public:
    explicit GlobalVariablesDialog(GlobalVariables* globals, QWidget* parent = nullptr);

private slots:
    void onAdd();
    void onRemove();
    void onCellChanged(int row, int column);

private:
    void refresh();
    GlobalVariables* m_globals = nullptr;
    QTableWidget* m_table = nullptr;
    bool m_updating = false;
};

} // namespace VisionInspector
