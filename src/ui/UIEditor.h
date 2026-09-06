/** @file UIEditor.h - 运行时界面编辑器 — 拖拽控件自定义运行界面布局，支持 JSON 存盘 */
#pragma once
#include <QWidget>
#include <QListWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QGraphicsRectItem>
#include <QLineEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace VisionInspector {

// ── 画布上代表一个已放置控件的图元 ──────────────────────────────────
class WidgetProxyItem : public QGraphicsRectItem {
public:
    enum WidgetType { ImageView, DataTable, Button, StatusPanel, ValueDisplay, Custom };

    explicit WidgetProxyItem(WidgetType type, const QString& id,
                             QGraphicsItem* parent = nullptr);

    QString widgetId() const { return m_widgetId; }
    WidgetType widgetType() const { return m_type; }
    QString label() const { return m_label; }
    void setLabel(const QString& label);

    /** 绑定: ValueDisplay=数据键(工具名.结果键) / Button=动作(run|start|stop); 其余类型未用 */
    QString bind() const { return m_bind; }
    void setBind(const QString& b) { m_bind = b; }

    // 序列化
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

    // 控件类型 ↔ 字符串
    static WidgetType typeFromString(const QString& s);
    static QString typeToString(WidgetType t);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

private:
    WidgetType m_type;
    QString m_widgetId;
    QString m_label;
    QString m_bind;
    QColor m_headerColor;

    void applyStyle();
};

// ── 接受拖放的设计画布 ──────────────────────────────────────────────
class DesignCanvas : public QGraphicsView {
    Q_OBJECT
public:
    explicit DesignCanvas(QGraphicsScene* scene, QWidget* parent = nullptr);

signals:
    void widgetDropped(const QString& type, const QPointF& scenePos);
    void selectionChanged(const QString& widgetId);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void onSceneSelectionChanged();
};

// ── 主编辑器 ────────────────────────────────────────────────────────
class UIEditor : public QWidget {
    Q_OBJECT
public:
    explicit UIEditor(QWidget* parent = nullptr);
    ~UIEditor() override;

    /// 保存布局到 JSON 文件
    bool saveLayout(const QString& filePath);

    /// 从 JSON 文件加载布局
    bool loadLayout(const QString& filePath);

    /// 导出为 JSON 字符串（供运行时引擎使用）
    QString toJson() const;

    /// 从 JSON 字符串恢复布局
    bool fromJson(const QString& json);

    /// 清空画布
    void clearCanvas();

    /// 设置画布尺寸
    void setCanvasSize(int width, int height);
    QSize canvasSize() const;

    /// 撤销/重做（保留接口）
    bool canUndo() const;
    bool canRedo() const;

signals:
    /// 布局发生变化
    void layoutChanged();

    /// 选中了某个控件
    void widgetSelected(const QString& widgetId);

public slots:
    /// 撤销
    void undo();

    /// 重做
    void redo();

    /// 删除选中的控件
    void deleteSelected();

private slots:
    void onPaletteItemClicked(QListWidgetItem* item);
    void onWidgetDropped(const QString& type, const QPointF& scenePos);
    void onCanvasSelectionChanged(const QString& widgetId);
    void onPropertyEdited();
    void onWidgetLabelEdited();
    void onBindEdited();

private:
    void setupUI();
    void setupPalette();
    void setupCanvas();
    void setupPropertiesPanel();
    void updatePropertiesFromItem(WidgetProxyItem* item);
    void clearPropertiesPanel();
    void addWidgetToCanvas(const QString& type, const QPointF& pos);
    WidgetProxyItem* findItemById(const QString& id) const;

    // ── 左侧：控件面板 ──
    QListWidget* m_palette = nullptr;

    // ── 中央：设计画布 ──
    QGraphicsScene* m_canvasScene = nullptr;
    DesignCanvas* m_canvasView = nullptr;
    QGraphicsRectItem* m_canvasBorder = nullptr;  // 画布边界示意

    // ── 右侧：属性面板 ──
    QWidget* m_propsPanel = nullptr;
    QGroupBox* m_propsGroup = nullptr;
    QLabel* m_propsHint = nullptr;
    QLineEdit* m_labelEdit = nullptr;
    QSpinBox* m_xSpin = nullptr;
    QSpinBox* m_ySpin = nullptr;
    QSpinBox* m_wSpin = nullptr;
    QSpinBox* m_hSpin = nullptr;
    QLabel* m_typeLabel = nullptr;
    QLineEdit* m_bindEdit = nullptr;   // 绑定键(数值显示)/动作(按钮)

    // ── 状态 ──
    QString m_selectedId;
    int m_widgetCounter = 0;
    int m_canvasW = 1280;
    int m_canvasH = 720;
    bool m_updatingProperties = false;  // 防止属性面板更新循环
};

} // namespace VisionInspector
