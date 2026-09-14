#include "FlowEditor.h"
#include "../engine/FlowEngine.h"
#include "../engine/ToolRegistry.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QScrollBar>
#include <QApplication>
#include <cmath>

namespace VisionInspector {

// ============================================================
// PortItem 实现
// ============================================================

PortItem::PortItem(PortType type, ToolNode* parentNode, QGraphicsItem* parent)
    : QGraphicsEllipseItem(parent)
    , m_portType(type)
    , m_parentNode(parentNode)
{
    constexpr qreal d = ToolNode::PORT_RADIUS * 2;
    setRect(-ToolNode::PORT_RADIUS, -ToolNode::PORT_RADIUS, d, d);
    setBrush(QColor(100, 100, 100));
    setPen(QPen(QColor(80, 80, 80), 1.5));
    setZValue(2);           // 在节点上层
    setAcceptHoverEvents(true);
    setCursor(Qt::CrossCursor);

    // 设置标志以接收鼠标事件
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, false);
}

int PortItem::toolIndex() const {
    return m_parentNode ? m_parentNode->toolIndex() : -1;
}

void PortItem::addConnection(ConnectionItem* conn) {
    if (!m_connections.contains(conn))
        m_connections.append(conn);
    update();
}

void PortItem::removeConnection(ConnectionItem* conn) {
    m_connections.removeAll(conn);
    update();
}

QPointF PortItem::centerScenePos() const {
    return mapToScene(0, 0);
}

void PortItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/,
                      QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 端口填充
    QColor fill = isConnected() ? QColor(74, 158, 255) : QColor(100, 100, 100);
    painter->setBrush(fill);
    painter->setPen(QPen(isConnected() ? QColor(74, 158, 255) : QColor(130, 130, 130), 1.5));
    painter->drawEllipse(rect());

    // 内圈高光
    if (isConnected()) {
        painter->setBrush(QColor(150, 200, 255, 80));
        painter->setPen(Qt::NoPen);
        qreal innerR = ToolNode::PORT_RADIUS * 0.45;
        painter->drawEllipse(QPointF(0, 0), innerR, innerR);
    }
}

// ============================================================
// ToolNode 实现
// ============================================================

ToolNode::ToolNode(int toolIndex, ITool* tool, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_toolIndex(toolIndex)
    , m_tool(tool)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setZValue(1);
    setCursor(Qt::ArrowCursor);

    // 创建端口
    m_inputPort  = new PortItem(PortItem::Input,  this, this);
    m_outputPort = new PortItem(PortItem::Output, this, this);

    // 端口位置：输入在左侧中间，输出在右侧中间
    qreal portXIn  = -ToolNode::PORT_RADIUS;
    qreal portXOut = ToolNode::NODE_WIDTH - ToolNode::PORT_RADIUS;
    qreal portY    = ToolNode::HEADER_HEIGHT + (ToolNode::NODE_HEIGHT - ToolNode::HEADER_HEIGHT) / 2.0;
    m_inputPort->setPos(portXIn, portY);
    m_outputPort->setPos(portXOut, portY);

    updateAppearance();
}

ToolNode::~ToolNode() {
    // 子端口和连线由 Qt 父对象系统或 FlowEditor 管理
}

QRectF ToolNode::boundingRect() const {
    qreal pad = 2.0;
    return QRectF(-pad, -pad, NODE_WIDTH + 2 * pad, NODE_HEIGHT + 2 * pad);
}

QPainterPath ToolNode::shape() const {
    QPainterPath path;
    path.addRoundedRect(0, 0, NODE_WIDTH, NODE_HEIGHT, CORNER_RADIUS, CORNER_RADIUS);
    return path;
}

void ToolNode::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    bool selected = (option->state & QStyle::State_Selected);
    QRectF body(0, 0, NODE_WIDTH, NODE_HEIGHT);
    QRectF header(0, 0, NODE_WIDTH, HEADER_HEIGHT);

    // ---- 阴影 ----
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 60));
    painter->drawRoundedRect(body.translated(2, 3), CORNER_RADIUS, CORNER_RADIUS);

    // ---- 主体 ----
    QColor bodyColor = m_highlighted ? QColor(60, 70, 80) :
                       selected     ? QColor(200, 224, 248) : QColor(235, 244, 252);
    painter->setBrush(bodyColor);
    painter->setPen(QPen(selected ? QColor(74, 158, 255) : QColor(70, 70, 75), 1.5));
    painter->drawRoundedRect(body, CORNER_RADIUS, CORNER_RADIUS);

    // ---- 头部（类别颜色条） ----
    QColor headerColor = colorForCategory(m_tool ? m_tool->category() : ToolCategory::System);
    QPainterPath headerPath;
    headerPath.addRoundedRect(header, CORNER_RADIUS, CORNER_RADIUS);
    // 只保留顶部圆角
    QPainterPath headerClip;
    headerClip.addRect(0, CORNER_RADIUS, NODE_WIDTH, HEADER_HEIGHT - CORNER_RADIUS);
    headerClip.addRoundedRect(header, CORNER_RADIUS, CORNER_RADIUS);
    painter->setClipRect(QRectF(0, 0, NODE_WIDTH, HEADER_HEIGHT));
    painter->setBrush(headerColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(header.adjusted(0, 0, 0, CORNER_RADIUS),
                             CORNER_RADIUS, CORNER_RADIUS);
    painter->setClipping(false);

    // ---- 状态指示灯 ----
    if (m_tool) {
        ToolStatus st = m_tool->status();
        if (st != ToolStatus::Idle) {
            QColor statusColor = colorForStatus(st);
            painter->setBrush(statusColor);
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPointF(NODE_WIDTH - 14, HEADER_HEIGHT / 2), 5, 5);
        }

        // 禁用标记
        if (!m_tool->isActive()) {
            painter->setPen(QPen(QColor(255, 80, 80, 180), 1.5));
            painter->drawLine(QPointF(NODE_WIDTH - 22, 4), QPointF(NODE_WIDTH - 6, HEADER_HEIGHT - 4));
        }
    }

    // ---- 头部文字 ----
    painter->setPen(QColor(240, 240, 240));
    QFont headerFont("Microsoft YaHei", 9, QFont::Bold);
    painter->setFont(headerFont);
    QString headerText = m_tool ? m_tool->instanceName() : QString("?");
    // 截断过长文本
    QFontMetrics fm(headerFont);
    if (fm.horizontalAdvance(headerText) > NODE_WIDTH - 30) {
        headerText = fm.elidedText(headerText, Qt::ElideRight, NODE_WIDTH - 30);
    }
    painter->drawText(QRectF(8, 0, NODE_WIDTH - 30, HEADER_HEIGHT),
                      Qt::AlignVCenter | Qt::AlignLeft, headerText);

    // ---- 索引编号 ----
    painter->setPen(QColor(160, 160, 170));
    QFont idxFont("Consolas", 7);
    painter->setFont(idxFont);
    painter->drawText(QRectF(0, 0, NODE_WIDTH - 8, HEADER_HEIGHT),
                      Qt::AlignVCenter | Qt::AlignRight,
                      QString("#%1").arg(m_toolIndex + 1));

    // ---- 主体文字（类型名） ----
    painter->setPen(QColor(180, 180, 190));
    QFont bodyFont("Microsoft YaHei", 8);
    painter->setFont(bodyFont);
    QString typeText = m_tool ? m_tool->displayName() : QString("");
    QRectF bodyTextRect(8, HEADER_HEIGHT + 2, NODE_WIDTH - 16, NODE_HEIGHT - HEADER_HEIGHT - 4);
    painter->drawText(bodyTextRect, Qt::AlignVCenter | Qt::AlignLeft, typeText);

    // ---- 选中高亮边框 ----
    if (selected) {
        painter->setBrush(Qt::NoBrush);
        QPen selPen(QColor(74, 158, 255, 200), 2.0);
        painter->setPen(selPen);
        painter->drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5),
                                 CORNER_RADIUS, CORNER_RADIUS);
    }
}

void ToolNode::updateAppearance() {
    update();
    // 端口位置随节点移动保持不变（相对坐标）
    // 连线更新由外部触发
}

void ToolNode::setHighlighted(bool hl) {
    if (m_highlighted != hl) {
        m_highlighted = hl;
        update();
    }
}

QColor ToolNode::colorForStatus(ToolStatus status) const {
    switch (status) {
    case ToolStatus::OK:      return QColor(0, 220, 0);
    case ToolStatus::NG:      return QColor(255, 80, 80);
    case ToolStatus::Running: return QColor(255, 200, 0);
    default:                  return QColor(120, 120, 120);
    }
}

QColor ToolNode::colorForCategory(ToolCategory cat) const {
    switch (cat) {
    case ToolCategory::Camera:        return QColor(70, 130, 200);   // 蓝色
    case ToolCategory::ImageProcess:  return QColor(70, 130, 200);   // 蓝色
    case ToolCategory::Detection:     return QColor(200, 130, 50);   // 橙色
    case ToolCategory::Geometry:      return QColor(50, 170, 100);   // 绿色
    case ToolCategory::Calibration:   return QColor(160, 100, 200);  // 紫色
    case ToolCategory::Communication: return QColor(180, 150, 50);   // 金色
    case ToolCategory::Logic:         return QColor(50, 160, 180);   // 青色
    case ToolCategory::System:        return QColor(100, 100, 110);  // 灰色
    default:                          return QColor(100, 100, 110);  // 灰色
    }
}

QVariant ToolNode::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        // 通知连线更新
        if (m_inputPort) {
            for (auto* conn : m_inputPort->connections())
                conn->updatePath();
        }
        if (m_outputPort) {
            for (auto* conn : m_outputPort->connections())
                conn->updatePath();
        }
        emit nodeMoved(m_toolIndex);
    }
    return QGraphicsObject::itemChange(change, value);
}

void ToolNode::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* /*event*/) {
    emit nodeDoubleClicked(m_toolIndex);
}

void ToolNode::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    // 让场景处理右键菜单，携带节点信息
    event->ignore();
}

// ============================================================
// ConnectionItem 实现
// ============================================================

ConnectionItem::ConnectionItem(PortItem* source, PortItem* target,
                                QGraphicsItem* parent)
    : QGraphicsPathItem(parent)
    , m_source(source)
    , m_target(target)
{
    setZValue(0);  // 在节点下层
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    QPen pen(QColor(74, 158, 255, 180), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    setPen(pen);
    setBrush(Qt::NoBrush);

    if (m_source) m_source->addConnection(this);
    if (m_target) m_target->addConnection(this);

    updatePath();
}

ConnectionItem::~ConnectionItem() {
    if (m_source) m_source->removeConnection(this);
    if (m_target) m_target->removeConnection(this);
}

QPainterPath ConnectionItem::buildBezier(QPointF start, QPointF end) {
    QPainterPath path;
    path.moveTo(start);

    qreal dx = qAbs(end.x() - start.x());
    qreal ctrlDist = qMax(dx * 0.5, 50.0);

    QPointF ctrl1(start.x() + ctrlDist, start.y());
    QPointF ctrl2(end.x()   - ctrlDist, end.y());

    path.cubicTo(ctrl1, ctrl2, end);
    return path;
}

void ConnectionItem::updatePath() {
    QPointF start, end;

    if (m_isTemporary) {
        start = m_source ? m_source->centerScenePos() : QPointF();
        end   = m_tempEnd;
    } else {
        start = m_source ? m_source->centerScenePos() : QPointF();
        end   = m_target ? m_target->centerScenePos() : QPointF();
    }

    setPath(buildBezier(start, end));
}

void ConnectionItem::setTemporaryEnd(QPointF pos) {
    m_isTemporary = true;
    m_tempEnd     = pos;
    updatePath();
    setPen(QPen(QColor(120, 160, 200, 180), 1.5, Qt::DashLine));
}

void ConnectionItem::finalizeConnection() {
    m_isTemporary = false;
    setPen(QPen(QColor(74, 158, 255, 180), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (m_target) m_target->addConnection(this);
    updatePath();
}

void ConnectionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (option->state & QStyle::State_Selected) {
        QPen selPen(QColor(255, 200, 50, 220), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(selPen);
    } else {
        painter->setPen(pen());
    }
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    // 绘制箭头（仅在非临时连线时）
    if (!m_isTemporary) {
        QPointF end = m_target ? m_target->centerScenePos() : path().currentPosition();
        qreal tangent;
        qreal pct = path().percentAtLength(path().length() - 1);
        tangent = path().angleAtPercent(pct);  // 返回角度

        // 简单三角箭头
        qreal arrowSize = 7.0;
        QPointF dir(std::cos(qDegreesToRadians(tangent)),
                     -std::sin(qDegreesToRadians(tangent)));
        QPointF normal(-dir.y(), dir.x());

        QPointF p1 = end - dir * arrowSize + normal * arrowSize * 0.5;
        QPointF p2 = end - dir * arrowSize - normal * arrowSize * 0.5;

        QPolygonF arrow;
        arrow << end << p1 << p2;
        painter->setBrush(pen().color());
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(arrow);
    }
}

QPainterPath ConnectionItem::shape() const {
    // 加宽命中区域便于选中
    QPainterPathStroker stroker;
    stroker.setWidth(10.0);
    return stroker.createStroke(path());
}

// ============================================================
// FlowScene 实现
// ============================================================

FlowScene::FlowScene(QObject* parent)
    : QGraphicsScene(parent)
{
    setBackgroundBrush(QColor(43, 43, 43));

    // 设置场景大小
    setSceneRect(-5000, -5000, 10000, 10000);
}

void FlowScene::startConnectionDrag(PortItem* fromPort) {
    if (!fromPort || fromPort->portType() != PortItem::Output) return;

    m_draggingConnection = true;
    m_dragSourcePort     = fromPort;

    m_tempConnection = new ConnectionItem(fromPort, nullptr);
    addItem(m_tempConnection);
}

void FlowScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_draggingConnection && m_tempConnection) {
        m_tempConnection->setTemporaryEnd(event->scenePos());
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void FlowScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (m_draggingConnection && m_tempConnection) {
        // 检查是否释放在输入端口上
        PortItem* targetPort = nullptr;
        QList<QGraphicsItem*> itemsUnder = items(event->scenePos());
        for (auto* item : itemsUnder) {
            auto* port = dynamic_cast<PortItem*>(item);
            if (port && port->portType() == PortItem::Input
                && port->parentNode() != m_dragSourcePort->parentNode()
                && !port->isConnected()) {
                targetPort = port;
                break;
            }
        }

        if (targetPort) {
            // 创建正式连线
            int fromIdx = m_dragSourcePort->toolIndex();
            int toIdx   = targetPort->toolIndex();
            emit connectionRequested(fromIdx, toIdx);
        }

        // 清理临时连线
        removeItem(m_tempConnection);
        delete m_tempConnection;
        m_tempConnection = nullptr;
        m_draggingConnection = false;
        m_dragSourcePort     = nullptr;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

// ============================================================
// FlowEditor 实现
// ============================================================

FlowEditor::FlowEditor(QWidget* parent) : QWidget(parent) {
    setupView();
    setupScene();
}

void FlowEditor::setupView() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new FlowView(this);
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setStyleSheet("background-color: #dcebf8; border: none;");
    m_view->setAcceptDrops(true);

    // 从工具箱拖入工具
    connect(m_view, &FlowView::toolDropped, this, &FlowEditor::toolDropped);

    // UI排查 P0-3: Delete键删除选中节点 (此前未连接, onSceneDeleteRequested永不触发)
    connect(m_view, &FlowView::deleteRequested, this, &FlowEditor::onSceneDeleteRequested);

    // 启用鼠标中键平移
    m_view->viewport()->setCursor(Qt::ArrowCursor);

    layout->addWidget(m_view);
}

void FlowEditor::setupScene() {
    m_scene = new FlowScene(this);
    m_view->setScene(m_scene);

    // 场景右键菜单
    connect(m_scene, &FlowScene::sceneContextMenu,
            this, &FlowEditor::onSceneContextMenu);

    // 连线请求
    connect(m_scene, &FlowScene::connectionRequested,
            this, &FlowEditor::onConnectionRequested);
}

void FlowEditor::setFlow(Flow* flow) {
    m_flow = flow;
    refresh();
}

void FlowEditor::clearScene() {
    // 先清理连线
    for (auto* conn : m_connections) {
        m_scene->removeItem(conn);
        delete conn;
    }
    m_connections.clear();

    // 再清理节点
    for (auto* node : m_nodes) {
        m_scene->removeItem(node);
        delete node;
    }
    m_nodes.clear();
}

void FlowEditor::createNode(int index, ITool* tool) {
    auto* node = new ToolNode(index, tool);
    m_scene->addItem(node);
    m_nodes.append(node);

    // 双击 → 编辑属性
    connect(node, &ToolNode::nodeDoubleClicked, this, [this](int idx) {
        emit toolDoubleClicked(idx);
    });

    // 节点移动
    connect(node, &ToolNode::nodeMoved, this, &FlowEditor::onNodeMoved);

    // 右键菜单：转发到场景级处理
    node->setAcceptTouchEvents(false);
}

void FlowEditor::autoLayout() {
    // 简单的竖向排列，列宽 220px，行高 100px
    constexpr qreal colWidth  = 220.0;
    constexpr qreal rowHeight = 90.0;
    constexpr qreal startX    = 50.0;
    constexpr qreal startY    = 30.0;
    constexpr int   cols      = 1;  // 单列布局

    // 查找位置补正范围
    QList<QPair<int,int>> correctionRanges;
    if (m_flow) {
        for (int i = 0; i < m_flow->toolCount(); ++i) {
            ITool* t = m_flow->toolAt(i);
            if (t && t->typeName() == "PositionCorrection") {
                int endIdx = -1;
                for (int j = i + 1; j < m_flow->toolCount(); ++j) {
                    ITool* t2 = m_flow->toolAt(j);
                    if (t2 && t2->typeName() == "EndCorrection") { endIdx = j; break; }
                }
                if (endIdx >= 0)
                    correctionRanges.append(qMakePair(i, endIdx));
            }
        }
    }

    auto isInCorrection = [&](int idx) -> bool {
        for (const auto& r : correctionRanges) {
            if (idx > r.first && idx < r.second) return true;
        }
        return false;
    };

    auto isCorrectionStart = [&](int idx) -> bool {
        for (const auto& r : correctionRanges)
            if (idx == r.first) return true;
        return false;
    };

    auto isCorrectionEnd = [&](int idx) -> bool {
        for (const auto& r : correctionRanges)
            if (idx == r.second) return true;
        return false;
    };

    for (int i = 0; i < m_nodes.size(); ++i) {
        int row = i / cols;
        int col = i % cols;

        qreal x = startX + col * colWidth;
        qreal y = startY + row * rowHeight;

        // 位置补正区域内的节点缩进
        if (isInCorrection(i)) {
            x += 40.0;
        } else if (isCorrectionStart(i) || isCorrectionEnd(i)) {
            x += 20.0;
        }

        m_nodes[i]->setPos(x, y);
    }
}

void FlowEditor::refresh() {
    clearScene();
    if (!m_flow) return;

    // 创建所有节点
    for (int i = 0; i < m_flow->toolCount(); ++i) {
        ITool* tool = m_flow->toolAt(i);
        if (!tool) continue;
        createNode(i, tool);
    }

    // 自动排列
    autoLayout();

    // 居中显示
    m_view->fitInView(m_scene->itemsBoundingRect().adjusted(-50, -50, 50, 50),
                      Qt::KeepAspectRatio);
    if (m_view->transform().m11() > 1.5) {
        m_view->resetTransform();
    }
}

void FlowEditor::updateToolStatus(int index, ToolStatus status) {
    auto* node = nodeAt(index);
    if (node) {
        node->tool()->setStatus(status);
        node->updateAppearance();
    }
}

ToolNode* FlowEditor::nodeAt(int index) const {
    for (auto* node : m_nodes) {
        if (node->toolIndex() == index) return node;
    }
    return nullptr;
}

void FlowEditor::getCorrectionRange(int startIdx, int& endIdx) const {
    endIdx = -1;
    if (!m_flow) return;
    for (int i = startIdx + 1; i < m_flow->toolCount(); ++i) {
        ITool* tool = m_flow->toolAt(i);
        if (tool && tool->typeName() == "EndCorrection") {
            endIdx = i;
            return;
        }
    }
}

// ---- 槽函数 ----

void FlowEditor::onSceneContextMenu(QPoint screenPos) {
    // 获取场景中选中的节点
    QList<QGraphicsItem*> selItems = m_scene->selectedItems();
    ToolNode* selectedNode = nullptr;
    int idx = -1;

    for (auto* item : selItems) {
        auto* node = dynamic_cast<ToolNode*>(item);
        if (node) {
            selectedNode = node;
            idx = node->toolIndex();
            break;
        }
    }

    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #f4f9ff; color: #24425f; border: 1px solid #9cc2e8; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; }"
        "QMenu::item:selected { background-color: #505050; }"
        "QMenu::separator { height: 1px; background: #9cc2e8; margin: 4px 8px; }"
    );

    if (selectedNode && idx >= 0) {
        menu.addAction("🔧 属性", [this, idx]() { emit toolEditProperties(idx); });
        menu.addAction("✏️ 重命名", [this, idx]() {
            if (!m_flow) return;
            ITool* t = m_flow->toolAt(idx);
            if (!t) return;
            bool ok;
            QString newName = QInputDialog::getText(this, "重命名工具",
                                                     "工具名称:", QLineEdit::Normal,
                                                     t->instanceName(), &ok);
            if (ok && !newName.trimmed().isEmpty()) {
                emit toolRename(idx, newName.trimmed());
            }
        });
        menu.addSeparator();
        menu.addAction("⬆ 上移", [this, idx]() { emit toolMoveUp(idx); });
        menu.addAction("⬇ 下移", [this, idx]() { emit toolMoveDown(idx); });
        menu.addSeparator();

        if (m_flow) {
            ITool* t = m_flow->toolAt(idx);
            if (t) {
                QString activeText = t->isActive() ? "⏸ 禁用" : "▶ 启用";
                menu.addAction(activeText, [this, idx]() { emit toolToggleActive(idx); });
            }
        }
        menu.addSeparator();
        menu.addAction("📋 复制", [this, idx]() { emit toolCopy(idx); });
        if (!m_clipboardTypeName.isEmpty()) {
            menu.addAction("📌 粘贴到此处", [this, idx]() { emit toolPaste(idx); });
        }
        menu.addAction("📌 在末尾粘贴", [this]() { emit toolPaste(-1); });
        menu.addSeparator();
        menu.addAction("🗑 删除", [this, idx]() { emit toolDelete(idx); });
    } else {
        // 空白处右键
        menu.addAction("🔄 刷新", [this]() { refresh(); });
        menu.addSeparator();
        if (!m_clipboardTypeName.isEmpty()) {
            menu.addAction("📌 粘贴", [this]() { emit toolPaste(-1); });
        }
        menu.addAction("🔍 适合窗口", [this]() {
            m_view->fitInView(m_scene->itemsBoundingRect().adjusted(-50, -50, 50, 50),
                              Qt::KeepAspectRatio);
        });
        menu.addAction("↺ 重置缩放", [this]() { m_view->resetTransform(); });
    }

    menu.exec(screenPos);
}

void FlowEditor::onConnectionRequested(int fromIdx, int toIdx) {
    // 验证有效性
    if (fromIdx < 0 || toIdx < 0 || fromIdx == toIdx) return;
    if (!m_flow) return;

    auto* fromNode = nodeAt(fromIdx);
    auto* toNode   = nodeAt(toIdx);
    if (!fromNode || !toNode) return;

    // 检查是否已有连线（单输出/单输入限制）
    if (toNode->inputPort()->isConnected()) return;

    // 创建连线
    auto* conn = new ConnectionItem(fromNode->outputPort(), toNode->inputPort());
    m_scene->addItem(conn);
    m_connections.append(conn);

    emit connectionCreated(fromIdx, toIdx);
}

void FlowEditor::onNodeMoved(int /*toolIndex*/) {
    // 连线更新在 ToolNode::itemChange 中自动处理
    // 此处可扩展为保存节点位置到配置
}

void FlowEditor::onSceneDeleteRequested() {
    // 删除选中的节点
    for (auto* node : m_nodes) {
        if (node->isSelected()) {
            int idx = node->toolIndex();
            emit toolDelete(idx);
        }
    }
}

// ============================================================
// FlowView 实现
// ============================================================

FlowView::FlowView(QWidget* parent) : QGraphicsView(parent) {
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(SmartViewportUpdate);
    setDragMode(RubberBandDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet("background-color: #dcebf8; border: none;");
}

// 接收从工具箱拖来的工具
void FlowView::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-lwvision-tool"))) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        QGraphicsView::dragEnterEvent(event);
    }
}

void FlowView::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-lwvision-tool"))) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void FlowView::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-lwvision-tool"))) {
        const QString typeName =
            QString::fromUtf8(event->mimeData()->data(QStringLiteral("application/x-lwvision-tool")));
        if (!typeName.isEmpty()) {
            emit toolDropped(typeName);
            event->setDropAction(Qt::CopyAction);
            event->accept();
            return;
        }
    }
    QGraphicsView::dropEvent(event);
}

void FlowView::wheelEvent(QWheelEvent* event) {
    // Ctrl+滚轮缩放
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
        scale(factor, factor);
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void FlowView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void FlowView::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void FlowView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void FlowView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        emit deleteRequested();
        event->accept();
    } else {
        QGraphicsView::keyPressEvent(event);
    }
}

// ============================================================
// FlowScene 补充实现
// ============================================================

void FlowScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsScene::mousePressEvent(event);
}

void FlowScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    emit sceneContextMenu(event->screenPos());
    event->accept();
}

// ============================================================
// ConnectionItem 补充实现
// ============================================================

void ConnectionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    QAction* deleteAction = menu.addAction("删除连线");
    QAction* selected = menu.exec(event->screenPos());
    if (selected == deleteAction) {
        // 从端口移除连接
        if (m_source) m_source->removeConnection(this);
        if (m_target) m_target->removeConnection(this);
        scene()->removeItem(this);
        delete this;
    }
    event->accept();
}

// ============================================================
// PortItem 补充实现
// ============================================================

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent* /*event*/) {
    m_hovered = true;
    update();
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* /*event*/) {
    m_hovered = false;
    update();
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 开始连线拖拽由 FlowScene 处理
        event->accept();
    } else {
        QGraphicsEllipseItem::mousePressEvent(event);
    }
}

void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsEllipseItem::mouseMoveEvent(event);
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsEllipseItem::mouseReleaseEvent(event);
}

} // namespace VisionInspector
