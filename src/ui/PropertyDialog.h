/** @file PropertyDialog.h - 工具属性编辑对话框（支持文件浏览） */
#pragma once
#include "../engine/ITool.h"
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>

namespace VisionInspector {

class PropertyDialog : public QDialog {
    Q_OBJECT
public:
    explicit PropertyDialog(ITool* tool, const QStringList& availableTools, QWidget* parent = nullptr);

private:
    ITool* m_tool;
    QStringList m_availableTools;
    QFormLayout* m_formLayout;
    QDialogButtonBox* m_buttonBox;
    QMap<QString, QWidget*> m_editors;

    void buildUI();
    void accept() override;

    /** 创建带浏览按钮的路径输入框 */
    QWidget* createPathEditor(const QString& name, const QString& value,
                              const QString& filter, bool isDir = false);
};

} // namespace VisionInspector
