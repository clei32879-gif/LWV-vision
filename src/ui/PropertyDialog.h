/** @file PropertyDialog.h - 工具属性编辑对话框
 *
 *  对齐CKVision工具对话框范式:
 *  - 属性表单(含路径浏览、数据链接"..."按钮)
 *  - 数据判定区: 对工具结果键配置上下限(启用后执行时自动判定OK/NG)
 */
#pragma once
#include "../engine/ITool.h"
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QTableWidget>

namespace VisionInspector {

class PropertyDialog : public QDialog {
    Q_OBJECT
public:
    /** @param availableTools 可用作输入图像的前序工具名列表
     *  @param linkableData   可链接数据: 工具实例名 -> 结果键列表 (来自上次运行) */
    explicit PropertyDialog(ITool* tool, const QStringList& availableTools,
                            const QMap<QString, QStringList>& linkableData = {},
                            QWidget* parent = nullptr);

private:
    ITool* m_tool;
    QStringList m_availableTools;
    QMap<QString, QStringList> m_linkableData;
    QFormLayout* m_formLayout;
    QDialogButtonBox* m_buttonBox;
    QMap<QString, QWidget*> m_editors;
    QTableWidget* m_judgeTable = nullptr;
    bool m_updatingJudge = false;

    void buildUI();
    void buildJudgeSection();
    void accept() override;

    /** 创建带浏览按钮的路径输入框 */
    QWidget* createPathEditor(const QString& name, const QString& value,
                              const QString& filter, bool isDir = false);
    /** 创建带数据链接按钮的文本输入框 (插入 "$(工具名.键)") */
    QWidget* createLinkEditor(const QString& name, const QString& value);
};

} // namespace VisionInspector
