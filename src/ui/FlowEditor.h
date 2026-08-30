/** @file FlowEditor.h - 节点式流程编辑器 (QGraphicsScene + QGraphicsView) */
#pragma once
#include "../engine/ITool.h"
#include "../engine/FlowEngine.h"
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QPen>
#include <QBrush>
#include <QPainter>
#include <QMenu>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QStyleOptionGraphicsItem>
#include <QStyle>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

namespace VisionInspector {

// 前向声明
class ToolNode;
class PortItem;
class ConnectionItem;
class FlowScene;

// ============================================================
// PortItem - 节点上的输入/输出端口
// ============================================================
class PortItem : public QGraphicsEllipseItem {
public:
    enum PortType { Input, Output };

    PortItem(PortType type, ToolNode* parentNode, QGraphicsItem* parent = nullptr);

    PortType portType() const { return m_portType; }
    ToolNode* parentNode() const { return m_parentNode; }
    int toolIndex() const;

    void addConnection(ConnectionItem* conn);
    void removeConnection(ConnectionItem* conn);
    QList<ConnectionItem*> connections() const { return m_connections; }
    bool isConnected() const { return !m_connections.isEmpty(); }

    QPointF centerScenePos() const;

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    PortType m_portType;
    ToolNode* m_parentNode;
    QList<ConnectionItem*> m_connections;
    bool m_hovered = false;
};

// ============================================================
// ToolNode - 表示一个工具节点的矩形
// ============================================================
class ToolNode : public QGraphicsObject {
    Q_OBJECT

public:
    static constexpr qreal NODE_WIDTH = 180.0;
    static constexpr qreal NODE_HEIGHT = 64.0;
    static constexpr qreal HEADER_HEIGHT = 26.0;
    static constexpr qreal PORT_RADIUS = 6.0;
    static constexpr qreal CORNER_RADIUS = 6.0;

    ToolNode(int toolIndex, ITool* tool, QGraphicsItem* parent = nullptr);
    ~ToolNode() override;

    int toolIndex() const { return m_toolIndex; }
    ITool* tool() const { return m_tool; }
    void setTool(ITool* tool) { m_tool = tool; update(); }

    PortItem* inputPort() const { return m_inputPort; }
    PortItem* outputPort() const { return m_outputPort; }

    void updateAppearance();
    void setHighlighted(bool hl);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    enum { Type = QGraphicsItem::UserType + 1 };
    int type() const override { return Type; }

signals:
    void nodeMoved(int toolIndex);
    void nodeDoubleClicked(int toolIndex);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    int m_toolIndex;
    ITool* m_tool;
    PortItem* m_inputPort;
    PortItem* m_outputPort;
    bool m_highlighted = false;

    QColor colorForStatus(ToolStatus status) const;
    QColor colorForCategory(ToolCategory cat) const;
};

// ============================================================
// ConnectionItem - 两个端口之间的贝塞尔曲线连线
// ============================================================
class ConnectionItem : public QGraphicsPathItem {
public:
    ConnectionItem(PortItem* source, PortItem* target,
                   QGraphicsItem* parent = nullptr);
    ~ConnectionItem() override;

    PortItem* sourcePort() const { return m_source; }
    PortItem* targetPort() const { return m_target; }

    void updatePath();
    void setTemporaryEnd(QPointF pos);
    void finalizeConnection();

    enum { Type = QGraphicsItem::UserType + 2 };
    int type() const override { return Type; }

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    QPainterPath shape() const override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    PortItem* m_source;
    PortItem* m_target;
    QPointF m_tempEnd;
    bool m_isTemporary = false;

    static QPainterPath buildBezier(QPointF start, QPointF end);
};

// ============================================================
// FlowView - 自定义 QGraphicsView 支持缩放和平移
// ============================================================
class FlowView : public QGraphicsView {
    Q_OBJECT

public:
    explicit FlowView(QWidget* parent = nullptr);

signals:
    void deleteRequested();
    void toolDropped(const QString& typeName);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    bool m_panning = false;
    QPoint m_lastPanPoint;
};

// ============================================================
// FlowScene - 自定义 QGraphicsScene 处理连线拖拽
// ============================================================
class FlowScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit FlowScene(QObject* parent = nullptr);

    void startConnectionDrag(PortItem* fromPort);
    void cancelConnectionDrag();

signals:
    void connectionRequested(int fromToolIndex, int toToolIndex);
    void sceneContextMenu(QPoint screenPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    bool m_draggingConnection = false;
    PortItem* m_dragSourcePort = nullptr;
    ConnectionItem* m_tempConnection = nullptr;
};

// ============================================================
// FlowEditor - 节点式流程编辑器主控件
// ============================================================
class FlowEditor : public QWidget {
    Q_OBJECT

public:
    explicit FlowEditor(QWidget* parent = nullptr);

    void setFlow(Flow* flow);
    void refresh();
    void updateToolStatus(int index, ToolStatus status);

    QString clipboardTypeName() const { return m_clipboardTypeName; }
    void setClipboardTypeName(const QString& name) { m_clipboardTypeName = name; }

    void getCorrectionRange(int startIdx, int& endIdx) const;

    FlowView* graphicsView() const { return m_view; }
    FlowScene* flowScene() const { return m_scene; }
    ToolNode* nodeAt(int index) const;

signals:
    void toolDoubleClicked(int index);
    void toolEditProperties(int index);
    void toolToggleActive(int index);
    void toolDelete(int index);
    void toolMoveUp(int index);
    void toolMoveDown(int index);
    void toolRename(int index, const QString& newName);
    void toolCopy(int index);
    void toolPaste(int index);

    void connectionCreated(int fromIndex, int toIndex);
    void toolDropped(const QString& typeName);

private slots:
    void onSceneDeleteRequested();
    void onSceneContextMenu(QPoint screenPos);
    void onConnectionRequested(int fromIdx, int toIdx);

private:
    void setupUI();
    void setupView();
    void setupScene();
    void createNode(int index, ITool* tool);
    void clearScene();
    void autoLayout();
    void onNodeMoved(int toolIndex);

    FlowScene* m_scene;
    FlowView* m_view;
    Flow* m_flow = nullptr;
    QString m_clipboardTypeName;

    QList<ToolNode*> m_nodes;
    QList<ConnectionItem*> m_connections;
};

} // namespace VisionInspector
