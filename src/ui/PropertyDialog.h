/** @file PropertyDialog.h - 工具属性编辑对话框（四页签, 对齐CKVision范式）
 *
 *  页签1 基本设置: 工具名/注释/启用
 *  页签2 参数设置: 工具属性表单(路径浏览、数据链接"..."按钮)
 *  页签3 数据判定: 对工具结果键配置上下限(启用后执行时自动判定OK/NG)
 *  页签4 试执行: 用最近一帧图像试跑工具, 预览检测结果与叠加图形
 */
#pragma once
#include "../engine/ITool.h"
#include "../engine/ToolContext.h"
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QTableWidget>
#include <QPlainTextEdit>

class QTabWidget;
class QCheckBox;

namespace VisionInspector {

class ImageViewWidget;

class PropertyDialog : public QDialog {
    Q_OBJECT
public:
    /** @param availableTools 可用作输入图像的前序工具名列表
     *  @param linkableData   可链接数据: 工具实例名 -> 结果键列表 (来自上次运行)
     *  @param lastImage      最近一帧图像 (页签4试执行用, 可为空) */
    explicit PropertyDialog(ITool* tool, const QStringList& availableTools,
                            const QMap<QString, QStringList>& linkableData = {},
                            const CvImagePtr& lastImage = {},
                            QWidget* parent = nullptr);

private:
    ITool* m_tool;
    QStringList m_availableTools;
    QMap<QString, QStringList> m_linkableData;
    CvImagePtr m_lastImage;

    QTabWidget* m_tabs = nullptr;
    // 页签1 基本设置
    QLineEdit* m_nameEdit = nullptr;
    QPlainTextEdit* m_commentEdit = nullptr;
    QCheckBox* m_activeCheck = nullptr;
    // 页签2 参数设置
    QFormLayout* m_formLayout = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QMap<QString, QWidget*> m_editors;
    // 页签3 数据判定
    QTableWidget* m_judgeTable = nullptr;

    void buildUI();
    void buildJudgeSection();
    void onTryRun();
    void accept() override;

    /** 创建带浏览按钮的路径输入框 */
    QWidget* createPathEditor(const QString& name, const QString& value,
                              const QString& filter, bool isDir = false);
    /** 创建带数据链接按钮的文本输入框 (插入 "$(工具名.键)") */
    QWidget* createLinkEditor(const QString& name, const QString& value);
};

} // namespace VisionInspector
