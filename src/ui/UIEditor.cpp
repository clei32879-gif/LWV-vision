/** @file UIEditor.cpp - 运行时界面编辑器实现 */
#include "UIEditor.h"
#include "../utils/Common.h"

#include <QSplitter>
#include <QToolBar>
#include <QToolButton>
#include <QHeaderView>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMimeData>
#include <QDrag>
#include <QPainter>
#include <QApplication>
#include <QStyle>

namespace VisionInspector {

// ═══════════════════════════════════════════════════════════════════════
// WidgetProxyItem 实现
// ═══════════════════════════════════════════════════════════════════════

WidgetProxyItem::WidgetProxyItem(WidgetType type, const QString& id, QGraphicsItem* parent)
    : QGraphicsRectItem(parent)
    , m_type(type)
    , m_widgetId(id)
{
    // 默认尺寸
    switch (type) {
    case ImageView:    setRect(0, 0, 320, 240); break;
    case DataTable:    setRect(0, 0, 400, 300); break;
    case Button:       setRect(0, 0, 120, 40);  break;
    case StatusPanel:  setRect(0, 0, 200, 120); break;
    case ValueDisplay: setRect(0, 0, 160, 80);  break;
    default:           setRect(0, 0, 200, 100); break;
    }

    m_label = typeToString(type);
    applyStyle();

    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
}

void WidgetProxyItem::applyStyle() {
    switch (m_type) {
    case ImageView:
        m_headerColor = QColor(80, 140, 200);   // 蓝色 — 图像
        break;
    case DataTable:
        m_headerColor = QColor(60, 160, 100);   // 绿色 — 数据表
        break;
    case Button:
        m_headerColor = QColor(200, 140, 60);   // 橙色 — 按钮
        break;
    case StatusPanel:
        m_headerColor = QColor(160, 100, 200);  // 紫色 — 状态
        break;
    case ValueDisplay:
        m_headerColor = QColor(200, 100, 120);  // 粉色 — 数值
        break;
    default:
        m_headerColor = QColor(140, 140, 140);  // 灰色
        break;
    }
}

void WidgetProxyItem::setLabel(const QString& label) {
    if (!label.trimmed().isEmpty())
        m_label = label.trimmed();
}

void WidgetProxyItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                             QWidget* widget) {
    Q_UNUSED(widget)
    QRectF r = rect();
    qreal headerH = 24.0;

    // ── 主体背景 ──
    QColor bg = isSelected() ? QColor(55, 55, 70) : QColor(45, 45, 50);
    painter->setPen(Qt::NoPen);
    painter->setBrush(bg);
    painter->drawRoundedRect(r, 4, 4);

    // ── 标题栏 ──
    QRectF headerRect(r.x(), r.y(), r.width(), headerH);
    painter->setBrush(m_headerColor);
    QPainterPath headerPath;
    headerPath.addRoundedRect(headerRect.adjusted(0, 0, 0, 2), 4, 4);
    // 底部两个角拉平
    headerPath.addRect(r.x(), r.y() + headerH - 4, r.width(), 4);
    painter->drawPath(headerPath.simplified());
    // 简单做法：画矩形覆盖圆角底部
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_headerColor);
    painter->drawRect(r.x(), r.y() + headerH - 4, r.width(), 4);

    // ── 标题文字 ──
    painter->setPen(Qt::white);
    QFont headerFont;
    headerFont.setPixelSize(12);
    headerFont.setBold(true);
    painter->setFont(headerFont);
    painter->drawText(headerRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                      m_label.isEmpty() ? typeToString(m_type) : m_label);

    // ── 类型图标文字（内容区占位） ──
    painter->setPen(QColor(120, 120, 130));
    QFont bodyFont;
    bodyFont.setPixelSize(11);
    painter->setFont(bodyFont);
    QRectF bodyRect(r.x(), r.y() + headerH, r.width(), r.height() - headerH);
    QString hint;
    switch (m_type) {
    case ImageView:    hint = QString::fromUtf8("🖼 图像显示区域"); break;
    case DataTable:    hint = QString::fromUtf8("📊 数据表格");     break;
    case Button:       hint = QString::fromUtf8("🔘 按钮");         break;
    case StatusPanel:  hint = QString::fromUtf8("📋 状态面板");     break;
    case ValueDisplay: hint = QString::fromUtf8("🔢 数值显示");     break;
    default:           hint = QString::fromUtf8("控件");            break;
    }
    painter->drawText(bodyRect, Qt::AlignCenter, hint);

    // ── 选中边框 ──
    if (isSelected()) {
        painter->setPen(QPen(QColor(100, 180, 255), 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

        // 四角缩放把手
        painter->setBrush(QColor(100, 180, 255));
        qreal handleSize = 7;
        QVector<QPointF> corners = {
            r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()
        };
        for (const auto& c : corners) {
            painter->drawRect(QRectF(c.x() - handleSize/2, c.y() - handleSize/2,
                                     handleSize, handleSize));
        }
    }
}

QVariant WidgetProxyItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && scene()) {
        // 限制在画布范围内
        QPointF newPos = value.toPointF();
        QRectF sr = scene()->sceneRect();
        QRectF br = rect();
        newPos.setX(qMax(sr.left(), qMin(newPos.x(), sr.right() - br.width())));
        newPos.setY(qMax(sr.top(), qMin(newPos.y(), sr.bottom() - br.height())));
        return newPos;
    }
    return QGraphicsRectItem::itemChange(change, value);
}

QJsonObject WidgetProxyItem::toJson() const {
    QJsonObject obj;
    obj["id"]       = m_widgetId;
    obj["type"]     = typeToString(m_type);
    obj["label"]    = m_label;
    obj["x"]        = pos().x();
    obj["y"]        = pos().y();
    obj["width"]    = rect().width();
    obj["height"]   = rect().height();
    return obj;
}

void WidgetProxyItem::fromJson(const QJsonObject& obj) {
    m_label = obj.value("label").toString(typeToString(m_type));
    qreal x = obj.value("x").toDouble(0);
    qreal y = obj.value("y").toDouble(0);
    qreal w = obj.value("width").toDouble(rect().width());
    qreal h = obj.value("height").toDouble(rect().height());
    setPos(x, y);
    setRect(0, 0, qMax(w, 60.0), qMax(h, 30.0));
    applyStyle();
}

WidgetProxyItem::WidgetType WidgetProxyItem::typeFromString(const QString& s) {
    if (s == "ImageView")    return ImageView;
    if (s == "DataTable")    return DataTable;
    if (s == "Button")       return Button;
    if (s == "StatusPanel")  return StatusPanel;
    if (s == "ValueDisplay") return ValueDisplay;
    return Custom;
}

QString WidgetProxyItem::typeToString(WidgetType t) {
    switch (t) {
    case ImageView:    return "ImageView";
    case DataTable:    return "DataTable";
    case Button:       return "Button";
    case StatusPanel:  return "StatusPanel";
    case ValueDisplay: return "ValueDisplay";
    default:           return "Custom";
    }
}

// ═══════════════════════════════════════════════════════════════════════
// DesignCanvas 实现
// ═══════════════════════════════════════════════════════════════════════

DesignCanvas::DesignCanvas(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setAcceptDrops(true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setRenderHint(QPainter::Antialiasing, true);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    setStyleSheet(R"(
        QGraphicsView {
            background-color: #1e1e1e;
            border: 1px solid #3c3c3c;
        }
    )");

    // 选中变化转发
    connect(scene, &QGraphicsScene::selectionChanged, this, &DesignCanvas::onSceneSelectionChanged);
}

void DesignCanvas::onSceneSelectionChanged() {
    auto selected = scene()->selectedItems();
    if (selected.isEmpty()) {
        emit selectionChanged(QString());
        return;
    }
    auto* item = dynamic_cast<WidgetProxyItem*>(selected.first());
    if (item)
        emit selectionChanged(item->widgetId());
    else
        emit selectionChanged(QString());
}

void DesignCanvas::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("application/x-vision-widget-type"))
        event->acceptProposedAction();
    else
        QGraphicsView::dragEnterEvent(event);
}

void DesignCanvas::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat("application/x-vision-widget-type"))
        event->acceptProposedAction();
    else
        QGraphicsView::dragMoveEvent(event);
}

void DesignCanvas::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasFormat("application/x-vision-widget-type")) {
        QString type = QString::fromUtf8(event->mimeData()->data("application/x-vision-widget-type"));
        QPointF scenePos = mapToScene(event->position().toPoint());
        emit widgetDropped(type, scenePos);
        event->acceptProposedAction();
    } else {
        QGraphicsView::dropEvent(event);
    }
}

void DesignCanvas::mousePressEvent(QMouseEvent* event) {
    // 点击空白区域取消选中
    QGraphicsView::mousePressEvent(event);
    if (!event->isAccepted() && event->button() == Qt::LeftButton) {
        scene()->clearSelection();
        emit selectionChanged(QString());
    }
}

// ═══════════════════════════════════════════════════════════════════════
// 可拖拽的调色板列表项
// ═══════════════════════════════════════════════════════════════════════

/** 调色板条目 — 支持拖拽到画布 */
class PaletteListWidget : public QListWidget {
    Q_OBJECT
public:
    using QListWidget::QListWidget;

protected:
    void startDrag(Qt::DropActions supportedActions) override {
        auto* item = currentItem();
        if (!item) return;

        QString typeName = item->data(Qt::UserRole).toString();
        auto* mimeData = new QMimeData();
        mimeData->setData("application/x-vision-widget-type", typeName.toUtf8());

        auto* drag = new QDrag(this);
        drag->setMimeData(mimeData);

        // 创建拖拽预览图标
        QPixmap pixmap(140, 40);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(80, 80, 90));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, 140, 40, 6, 6);
        p.setPen(Qt::white);
        p.setFont(item->font());
        p.drawText(QRect(8, 0, 124, 40), Qt::AlignVCenter, item->text());
        p.end();
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(70, 20));

        drag->exec(Qt::CopyAction);
    }
};

// ═══════════════════════════════════════════════════════════════════════
// UIEditor 实现
// ═══════════════════════════════════════════════════════════════════════

UIEditor::UIEditor(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

UIEditor::~UIEditor() = default;

// ── 整体布局 ──────────────────────────────────────────────────────────

void UIEditor::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 顶部工具栏 ──
    auto* toolbar = new QToolBar(this);
    toolbar->setStyleSheet(R"(
        QToolBar {
            background-color: #333;
            border: none;
            padding: 2px 4px;
            spacing: 4px;
        }
        QToolButton {
            background-color: #3c3c3c;
            color: #ddd;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 4px 12px;
            font-size: 13px;
        }
        QToolButton:hover {
            background-color: #505050;
            border-color: #777;
        }
        QToolButton:pressed {
            background-color: #2a2a2a;
        }
    )");

    auto* btnSave = toolbar->addAction(QString::fromUtf8("💾 保存布局"));
    auto* btnLoad = toolbar->addAction(QString::fromUtf8("📂 加载布局"));
    toolbar->addSeparator();
    auto* btnClear = toolbar->addAction(QString::fromUtf8("🗑 清空画布"));
    toolbar->addSeparator();
    auto* btnUndo = toolbar->addAction(QString::fromUtf8("↩ 撤销"));
    auto* btnRedo = toolbar->addAction(QString::fromUtf8("↪ 重做"));
    toolbar->addSeparator();
    auto* btnDelete = toolbar->addAction(QString::fromUtf8("✕ 删除选中"));

    mainLayout->addWidget(toolbar);

    // ── 主分割区域：调色板 | 画布 | 属性面板 ──
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setStyleSheet("QSplitter::handle { background-color: #444; width: 2px; }");

    setupPalette();
    setupCanvas();
    setupPropertiesPanel();

    splitter->addWidget(m_palette);
    splitter->addWidget(m_canvasView);
    splitter->addWidget(m_propsPanel);
    splitter->setStretchFactor(0, 0);  // 调色板不拉伸
    splitter->setStretchFactor(1, 1);  // 画布拉伸
    splitter->setStretchFactor(2, 0);  // 属性面板不拉伸
    splitter->setSizes({180, 700, 220});

    mainLayout->addWidget(splitter, 1);

    // ── 连接信号 ──
    connect(btnSave,   &QAction::triggered, [this]() {
        QString path = QFileDialog::getSaveFileName(this,
            QString::fromUtf8("保存界面布局"), QString(), "JSON (*.json)");
        if (!path.isEmpty()) saveLayout(path);
    });
    connect(btnLoad,   &QAction::triggered, [this]() {
        QString path = QFileDialog::getOpenFileName(this,
            QString::fromUtf8("加载界面布局"), QString(), "JSON (*.json)");
        if (!path.isEmpty()) loadLayout(path);
    });
    connect(btnClear,  &QAction::triggered, this, &UIEditor::clearCanvas);
    connect(btnUndo,   &QAction::triggered, this, &UIEditor::undo);
    connect(btnRedo,   &QAction::triggered, this, &UIEditor::redo);
    connect(btnDelete, &QAction::triggered, this, &UIEditor::deleteSelected);

    connect(m_canvasView, &DesignCanvas::widgetDropped,
            this, &UIEditor::onWidgetDropped);
    connect(m_canvasView, &DesignCanvas::selectionChanged,
            this, &UIEditor::onCanvasSelectionChanged);

    // 设置默认画布尺寸
    setCanvasSize(m_canvasW, m_canvasH);
}

// ── 控件调色板 ───────────────────────────────────────────────────────

void UIEditor::setupPalette() {
    m_palette = new PaletteListWidget(this);
    m_palette->setDragEnabled(true);
    m_palette->setDragDropMode(QAbstractItemView::DragOnly);
    m_palette->setMaximumWidth(200);
    m_palette->setMinimumWidth(140);
    m_palette->setIconSize(QSize(24, 24));
    m_palette->setSpacing(2);
    m_palette->setStyleSheet(R"(
        QListWidget {
            background-color: #2b2b2b;
            color: #ddd;
            border: 1px solid #3c3c3c;
            font-size: 13px;
            outline: none;
        }
        QListWidget::item {
            padding: 10px 8px;
            border-bottom: 1px solid #383838;
        }
        QListWidget::item:hover {
            background-color: #3a3a3a;
        }
        QListWidget::item:selected {
            background-color: #4a6a8a;
        }
    )");

    // 控件类型定义：名称, 类型标识, 图标
    struct WidgetDef {
        QString label;
        QString typeName;
        QString icon;
    };
    QVector<WidgetDef> defs = {
        {QString::fromUtf8("🖼  图像视图"),     "ImageView",    QString::fromUtf8("📷")},
        {QString::fromUtf8("📊  数据表格"),     "DataTable",    QString::fromUtf8("📋")},
        {QString::fromUtf8("🔘  按钮"),         "Button",       QString::fromUtf8("🔘")},
        {QString::fromUtf8("📋  状态面板"),     "StatusPanel",  QString::fromUtf8("📊")},
        {QString::fromUtf8("🔢  数值显示"),     "ValueDisplay", QString::fromUtf8("🔢")},
    };

    for (const auto& def : defs) {
        auto* item = new QListWidgetItem(def.label);
        item->setData(Qt::UserRole, def.typeName);
        item->setToolTip(QString::fromUtf8("拖拽到画布添加 %1").arg(def.label));
        QFont f = item->font();
        f.setPixelSize(13);
        item->setFont(f);
        m_palette->addItem(item);
    }

    // 双击也添加到画布
    connect(m_palette, &QListWidget::itemDoubleClicked, this, &UIEditor::onPaletteItemClicked);
}

void UIEditor::onPaletteItemClicked(QListWidgetItem* item) {
    if (!item) return;
    QString type = item->data(Qt::UserRole).toString();
    // 默认放置到画布中央偏上的位置
    QPointF center(m_canvasW / 2.0 - 100, 60 + m_widgetCounter * 30);
    addWidgetToCanvas(type, center);
}

// ── 设计画布 ─────────────────────────────────────────────────────────

void UIEditor::setupCanvas() {
    m_canvasScene = new QGraphicsScene(this);
    m_canvasView  = new DesignCanvas(m_canvasScene, this);
}

void UIEditor::setCanvasSize(int width, int height) {
    m_canvasW = qMax(width, 400);
    m_canvasH = qMax(height, 300);
    m_canvasScene->setSceneRect(0, 0, m_canvasW, m_canvasH);

    // 重绘画布边框示意
    if (m_canvasBorder) {
        m_canvasScene->removeItem(m_canvasBorder);
        delete m_canvasBorder;
    }
    m_canvasBorder = m_canvasScene->addRect(0, 0, m_canvasW, m_canvasH,
                                             QPen(QColor(80, 80, 85), 2, Qt::DashLine),
                                             QBrush(QColor(30, 30, 32)));
    m_canvasBorder->setZValue(-1);

    // 居中显示
    m_canvasView->fitInView(m_canvasScene->sceneRect(), Qt::KeepAspectRatio);
}

QSize UIEditor::canvasSize() const {
    return QSize(m_canvasW, m_canvasH);
}

// ── 属性面板 ─────────────────────────────────────────────────────────

void UIEditor::setupPropertiesPanel() {
    m_propsPanel = new QWidget(this);
    m_propsPanel->setMaximumWidth(260);
    m_propsPanel->setMinimumWidth(180);
    m_propsPanel->setStyleSheet(R"(
        QWidget#propsPanel {
            background-color: #2b2b2b;
            border-left: 1px solid #3c3c3c;
        }
    )");
    m_propsPanel->setObjectName("propsPanel");

    auto* layout = new QVBoxLayout(m_propsPanel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // ── 属性标题 ──
    auto* titleLabel = new QLabel(QString::fromUtf8("📐 属性面板"));
    titleLabel->setStyleSheet("color: #aaa; font-size: 14px; font-weight: bold; padding: 4px 0;");
    layout->addWidget(titleLabel);

    // ── 提示文本（未选中时显示） ──
    m_propsHint = new QLabel(QString::fromUtf8("从左侧拖拽控件到画布\n或点击画布上的控件查看属性"));
    m_propsHint->setStyleSheet("color: #666; font-size: 12px; padding: 20px 8px;");
    m_propsHint->setAlignment(Qt::AlignCenter);
    m_propsHint->setWordWrap(true);
    layout->addWidget(m_propsHint);

    // ── 属性分组 ──
    m_propsGroup = new QGroupBox(QString::fromUtf8("控件属性"));
    m_propsGroup->setStyleSheet(R"(
        QGroupBox {
            color: #bbb;
            font-weight: bold;
            border: 1px solid #444;
            border-radius: 4px;
            margin-top: 12px;
            padding-top: 16px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 6px;
        }
        QLabel {
            color: #aaa;
            font-size: 12px;
        }
        QLineEdit, QSpinBox, QComboBox {
            background-color: #3c3c3c;
            color: #ddd;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 3px 6px;
            font-size: 12px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #6a9fd8;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #444;
            border: none;
            width: 16px;
        }
    )");
    m_propsGroup->setVisible(false);
    layout->addWidget(m_propsGroup);

    auto* form = new QFormLayout(m_propsGroup);
    form->setSpacing(6);

    // 类型（只读展示）
    m_typeLabel = new QLabel("-");
    m_typeLabel->setStyleSheet("color: #8ac; font-weight: bold;");
    form->addRow(QString::fromUtf8("类型:"), m_typeLabel);

    // 名称
    m_labelEdit = new QLineEdit();
    m_labelEdit->setPlaceholderText(QString::fromUtf8("控件名称"));
    form->addRow(QString::fromUtf8("名称:"), m_labelEdit);
    connect(m_labelEdit, &QLineEdit::editingFinished, this, &UIEditor::onWidgetLabelEdited);

    // 位置 X
    m_xSpin = new QSpinBox();
    m_xSpin->setRange(0, 4000);
    m_xSpin->setSuffix(" px");
    form->addRow("X:", m_xSpin);

    // 位置 Y
    m_ySpin = new QSpinBox();
    m_ySpin->setRange(0, 4000);
    m_ySpin->setSuffix(" px");
    form->addRow("Y:", m_ySpin);

    // 宽度
    m_wSpin = new QSpinBox();
    m_wSpin->setRange(30, 4000);
    m_wSpin->setSuffix(" px");
    form->addRow(QString::fromUtf8("宽:"), m_wSpin);

    // 高度
    m_hSpin = new QSpinBox();
    m_hSpin->setRange(20, 4000);
    m_hSpin->setSuffix(" px");
    form->addRow(QString::fromUtf8("高:"), m_hSpin);

    // ── 连接属性编辑信号 ──
    connect(m_xSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &UIEditor::onPropertyEdited);
    connect(m_ySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &UIEditor::onPropertyEdited);
    connect(m_wSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &UIEditor::onPropertyEdited);
    connect(m_hSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &UIEditor::onPropertyEdited);

    layout->addStretch();

    // ── 底部删除按钮 ──
    auto* btnDeleteWidget = new QPushButton(QString::fromUtf8("🗑 删除控件"));
    btnDeleteWidget->setStyleSheet(R"(
        QPushButton {
            background-color: #8b3a3a;
            color: #ddd;
            border: none;
            border-radius: 4px;
            padding: 8px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #a04040;
        }
        QPushButton:pressed {
            background-color: #6e2e2e;
        }
    )");
    btnDeleteWidget->setVisible(false);
    connect(btnDeleteWidget, &QPushButton::clicked, this, &UIEditor::deleteSelected);
    layout->addWidget(btnDeleteWidget);

    // 保存引用以便控制可见性
    btnDeleteWidget->setObjectName("btnDeleteWidget");
}

// ── 添加控件到画布 ───────────────────────────────────────────────────

void UIEditor::addWidgetToCanvas(const QString& type, const QPointF& pos) {
    auto wtype = WidgetProxyItem::typeFromString(type);
    QString id = QString("widget_%1").arg(++m_widgetCounter);

    auto* item = new WidgetProxyItem(wtype, id);
    item->setPos(pos);

    // 计算默认放置位置（避免完全重叠）
    QPointF adjustedPos = pos;
    // 检查是否与已有控件重叠，微调
    const auto& items = m_canvasScene->items();
    bool overlap = true;
    int tries = 0;
    while (overlap && tries < 50) {
        overlap = false;
        for (auto* other : items) {
            auto* proxy = dynamic_cast<WidgetProxyItem*>(other);
            if (!proxy || proxy == item) continue;
            QRectF r1(adjustedPos, item->rect().size());
            QRectF r2(proxy->pos(), proxy->rect().size());
            if (r1.intersects(r2)) {
                overlap = true;
                adjustedPos += QPointF(30, 30);
                break;
            }
        }
        tries++;
    }

    item->setPos(adjustedPos);
    m_canvasScene->addItem(item);
    m_canvasScene->clearSelection();
    item->setSelected(true);

    emit layoutChanged();
}

void UIEditor::onWidgetDropped(const QString& type, const QPointF& scenePos) {
    // 居中放置（以鼠标位置为控件左上角）
    QPointF pos(scenePos.x() - 50, scenePos.y() - 15);
    addWidgetToCanvas(type, pos);
}

// ── 选中处理 ─────────────────────────────────────────────────────────

void UIEditor::onCanvasSelectionChanged(const QString& widgetId) {
    m_selectedId = widgetId;

    auto* item = findItemById(widgetId);
    if (item) {
        updatePropertiesFromItem(item);
        emit widgetSelected(widgetId);
    } else {
        clearPropertiesPanel();
    }
}

WidgetProxyItem* UIEditor::findItemById(const QString& id) const {
    if (id.isEmpty()) return nullptr;
    for (auto* qitem : m_canvasScene->items()) {
        auto* item = dynamic_cast<WidgetProxyItem*>(qitem);
        if (item && item->widgetId() == id)
            return item;
    }
    return nullptr;
}

void UIEditor::updatePropertiesFromItem(WidgetProxyItem* item) {
    if (!item) { clearPropertiesPanel(); return; }

    m_updatingProperties = true;

    m_propsHint->setVisible(false);
    m_propsGroup->setVisible(true);

    // 显示删除按钮
    auto* btnDelete = m_propsPanel->findChild<QPushButton*>("btnDeleteWidget");
    if (btnDelete) btnDelete->setVisible(true);

    m_typeLabel->setText(WidgetProxyItem::typeToString(item->widgetType()));
    m_labelEdit->setText(item->label());

    QRectF r = item->rect();
    QPointF p = item->pos();

    m_xSpin->blockSignals(true);
    m_ySpin->blockSignals(true);
    m_wSpin->blockSignals(true);
    m_hSpin->blockSignals(true);

    m_xSpin->setValue(static_cast<int>(p.x()));
    m_ySpin->setValue(static_cast<int>(p.y()));
    m_wSpin->setValue(static_cast<int>(r.width()));
    m_hSpin->setValue(static_cast<int>(r.height()));

    m_xSpin->blockSignals(false);
    m_ySpin->blockSignals(false);
    m_wSpin->blockSignals(false);
    m_hSpin->blockSignals(false);

    m_updatingProperties = false;
}

void UIEditor::clearPropertiesPanel() {
    m_propsHint->setVisible(true);
    m_propsGroup->setVisible(false);
    auto* btnDelete = m_propsPanel->findChild<QPushButton*>("btnDeleteWidget");
    if (btnDelete) btnDelete->setVisible(false);
    m_labelEdit->clear();
    m_selectedId.clear();
}

void UIEditor::onPropertyEdited() {
    if (m_updatingProperties) return;

    auto* item = findItemById(m_selectedId);
    if (!item) return;

    item->setPos(m_xSpin->value(), m_ySpin->value());
    item->setRect(0, 0, m_wSpin->value(), m_hSpin->value());
    item->update();

    emit layoutChanged();
}

void UIEditor::onWidgetLabelEdited() {
    auto* item = findItemById(m_selectedId);
    if (!item) return;
    item->setLabel(m_labelEdit->text());
    item->update();
    emit layoutChanged();
}

// ── 删除控件 ─────────────────────────────────────────────────────────

void UIEditor::deleteSelected() {
    if (m_selectedId.isEmpty()) return;

    auto* item = findItemById(m_selectedId);
    if (!item) return;

    m_canvasScene->removeItem(item);
    delete item;
    m_selectedId.clear();
    clearPropertiesPanel();

    emit layoutChanged();
}

// ── 清空画布 ─────────────────────────────────────────────────────────

void UIEditor::clearCanvas() {
    // 只清除控件，保留画布边框
    QList<QGraphicsItem*> toRemove;
    for (auto* qitem : m_canvasScene->items()) {
        if (dynamic_cast<WidgetProxyItem*>(qitem))
            toRemove.append(qitem);
    }
    for (auto* item : toRemove) {
        m_canvasScene->removeItem(item);
        delete item;
    }
    m_selectedId.clear();
    m_widgetCounter = 0;
    clearPropertiesPanel();

    emit layoutChanged();
}

// ── 撤销 / 重做 ──────────────────────────────────────────────────────

void UIEditor::undo() {
    // 保留接口 — 后续可扩展为 QUndoStack 完整实现
}

void UIEditor::redo() {
    // 保留接口 — 后续可扩展为 QUndoStack 完整实现
}

bool UIEditor::canUndo() const { return false; }
bool UIEditor::canRedo() const { return false; }

// ═══════════════════════════════════════════════════════════════════════
// JSON 序列化 / 反序列化
// ═══════════════════════════════════════════════════════════════════════

QString UIEditor::toJson() const {
    QJsonObject root;
    root["version"]    = 1;
    root["canvasWidth"]  = m_canvasW;
    root["canvasHeight"] = m_canvasH;

    QJsonArray widgets;
    for (auto* qitem : m_canvasScene->items()) {
        auto* item = dynamic_cast<WidgetProxyItem*>(qitem);
        if (!item) continue;
        widgets.append(item->toJson());
    }
    root["widgets"] = widgets;

    QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

bool UIEditor::fromJson(const QString& json) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning("UIEditor::fromJson — JSON 解析错误: %s", qPrintable(error.errorString()));
        return false;
    }

    QJsonObject root = doc.object();

    // 版本检查
    int version = root.value("version").toInt(1);
    Q_UNUSED(version)

    // 清除现有控件
    clearCanvas();

    // 恢复画布尺寸
    int cw = root.value("canvasWidth").toInt(1280);
    int ch = root.value("canvasHeight").toInt(720);
    setCanvasSize(cw, ch);

    // 恢复控件
    QJsonArray widgets = root.value("widgets").toArray();
    for (const auto& val : widgets) {
        QJsonObject obj = val.toObject();
        QString typeStr = obj.value("type").toString("Custom");
        QString id       = obj.value("id").toString();

        auto wtype = WidgetProxyItem::typeFromString(typeStr);
        auto* item = new WidgetProxyItem(wtype, id);
        item->fromJson(obj);
        m_canvasScene->addItem(item);

        // 恢复计数器（解析 id 中的数字）
        static QRegularExpression numRe("widget_(\\d+)");
        auto match = numRe.match(id);
        if (match.hasMatch()) {
            int num = match.captured(1).toInt();
            if (num > m_widgetCounter) m_widgetCounter = num;
        }
    }

    emit layoutChanged();
    return true;
}

bool UIEditor::saveLayout(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(const_cast<UIEditor*>(this),
                             QString::fromUtf8("保存失败"),
                             QString::fromUtf8("无法写入文件:\n%1").arg(filePath));
        return false;
    }
    file.write(toJson().toUtf8());
    file.close();
    return true;
}

bool UIEditor::loadLayout(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             QString::fromUtf8("加载失败"),
                             QString::fromUtf8("无法读取文件:\n%1").arg(filePath));
        return false;
    }
    QString json = QString::fromUtf8(file.readAll());
    file.close();

    if (!fromJson(json)) {
        QMessageBox::warning(this,
                             QString::fromUtf8("加载失败"),
                             QString::fromUtf8("JSON 格式无效"));
        return false;
    }
    return true;
}

} // namespace VisionInspector

#include "UIEditor.moc"
