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
class QTimer;
class QVBoxLayout;

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
    // 页签3 数据判定 (容器+布局, 支持试执行后重建)
    QWidget* m_judgeContainer = nullptr;
    QVBoxLayout* m_judgeLayout = nullptr;
    QTableWidget* m_judgeTable = nullptr;
    // 页签4 试执行 (即改即显: 参数变化自动重跑)
    QTimer* m_previewTimer = nullptr;
    ImageViewWidget* m_previewViewer = nullptr;
    // ROI 图形化编辑 (中心约定 roiCenterX/Y+roiWidth/Height 或 角点约定 useROI+roiX/Y/W/H)
    class QPushButton* m_roiEditBtn = nullptr;
    class QCheckBox* m_showOverlayCheck = nullptr;   // 显示选项页: 检测图形开关
    class ProfileChart* m_profileChart = nullptr;    // 试执行剖面曲线
    QString m_okColor, m_ngColor;                    // OK/NG 叠加颜色
    bool m_hasRoi = false;
    bool m_roiCornerMode = false; // true=角点约定 (条码/二维码/OCR 等老工具)

    void buildUI();
    void buildJudgeSection();
    void onTryRun();
    /** 试执行并把结果叠加到预览视图 (参数变化自动触发) */
    void updatePreview();
    /** 把参数编辑器的变化信号接到预览防抖定时器 */
    void connectAutoPreview(QWidget* editor);
    /** 编辑器数值 → 预览图ROI框 */
    void syncRoiToViewer();
    /** 图上拖拽ROI → 写回数值编辑器 (触发自动预览) */
    void onRoiEdited(const QRectF& rect);
    void accept() override;

    /** 创建带浏览按钮的路径输入框 */
    QWidget* createPathEditor(const QString& name, const QString& value,
                              const QString& filter, bool isDir = false);
    /** 创建带数据链接按钮的文本输入框 (插入 "$(工具名.键)") */
    QWidget* createLinkEditor(const QString& name, const QString& value);
};

} // namespace VisionInspector
