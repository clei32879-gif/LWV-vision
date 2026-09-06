/** @file RuntimeUI.h - DIY 布局的运行时渲染器 (阶段6: DIY界面编辑器完善)
 *
 *  读取 UIEditor 保存的布局 JSON (config/runtime_ui.json), 生成真实可用的
 *  操作员运行界面:
 *    ImageView   → 图像查看控件 (实时检测图)
 *    DataTable   → 检测结果表 (工具/结果)
 *    ValueDisplay→ 数值显示 (绑定键 "工具名.结果键", 随执行刷新)
 *    Button      → 操作按钮 (绑定动作 run/start/stop)
 *    StatusPanel → 工位状态灯面板 (复用 widgets/StatusPanel)
 */
#pragma once
#include "../utils/Common.h"
#include <QWidget>
#include <QVector>

class QTableWidget;
class QScrollArea;
class QLabel;

namespace VisionInspector {

class ImageViewWidget;
class StatusPanel;

class RuntimeUI : public QWidget {
    Q_OBJECT
public:
    explicit RuntimeUI(QWidget* parent = nullptr);

    /** 从布局JSON加载 (与 UIEditor 保存格式一致); 失败返回false */
    bool loadLayout(const QString& path);

    /** 最近帧图像 → 图像控件 */
    void setRuntimeImage(const QImage& img);

    /** 执行结果 → 数据表 + 数值显示 (toolStates: 工具名/是否OK) */
    void updateResults(const QList<QPair<QString, bool>>& toolStates,
                       const DataMap& resultData);

    /** 工位状态 → 状态灯面板 */
    void setStationStatus(int index, bool ok, const QString& name);

signals:
    /** 按钮动作: "run"(单次执行) / "start"(连续启动) / "stop"(停止) */
    void actionRequested(const QString& action);

private:
    QWidget* buildWidget(const QString& type, const QString& label,
                         const QString& bind, const QRectF& geo);

    QWidget* m_canvas = nullptr;
    ImageViewWidget* m_imageView = nullptr;
    QTableWidget* m_table = nullptr;
    StatusPanel* m_statusPanel = nullptr;

    struct ValueBind {
        QLabel* label = nullptr;   // 数值标签
        QString key;               // 绑定键: 工具名.结果键
    };
    QVector<ValueBind> m_valueBinds;
};

} // namespace VisionInspector
