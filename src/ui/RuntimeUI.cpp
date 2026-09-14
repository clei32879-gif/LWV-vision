/** @file RuntimeUI.cpp - DIY 布局运行时渲染器实现 */
#include "RuntimeUI.h"
#include "widgets/ImageViewWidget.h"
#include "widgets/StatusPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFont>

namespace VisionInspector {

RuntimeUI::RuntimeUI(QWidget* parent)
    : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setWidgetResizable(false);
    scroll->setStyleSheet("background-color: #e4eef8; border: none;");
    outer->addWidget(scroll, 1);
    m_canvas = new QWidget(scroll);
    m_canvas->setStyleSheet("background-color: #e4eef8;");
    scroll->setWidget(m_canvas);
}

bool RuntimeUI::loadLayout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError err;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll(), &err).object();
    file.close();
    if (err.error != QJsonParseError::NoError) return false;

    const int cw = root.value("canvasWidth").toInt(1280);
    const int ch = root.value("canvasHeight").toInt(720);
    m_canvas->setFixedSize(cw, ch);

    // 清掉旧控件 (canvas 自身布局为绝对定位, 直接 delete 子控件)
    const auto oldChildren = m_canvas->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* w : oldChildren)
        if (w != m_canvas) { w->setParent(nullptr); w->deleteLater(); }
    m_imageView = nullptr;
    m_table = nullptr;
    m_statusPanel = nullptr;
    m_valueBinds.clear();

    const QJsonArray widgets = root.value("widgets").toArray();
    for (const QJsonValue& wv : widgets) {
        const QJsonObject o = wv.toObject();
        const QString type = o.value("type").toString();
        const QString label = o.value("label").toString(type);
        const QString bind = o.value("bind").toString();
        const QRectF geo(o.value("x").toDouble(), o.value("y").toDouble(),
                         qMax(60.0, o.value("width").toDouble()),
                         qMax(30.0, o.value("height").toDouble()));
        QWidget* w = buildWidget(type, label, bind, geo);
        if (w) {
            w->setParent(m_canvas);
            w->setGeometry(geo.toRect());
            w->show();
        }
    }
    return true;
}

QWidget* RuntimeUI::buildWidget(const QString& type, const QString& label,
                                const QString& bind, const QRectF& geo) {
    if (type == QStringLiteral("ImageView")) {
        m_imageView = new ImageViewWidget(m_canvas);
        return m_imageView;
    }

    if (type == QStringLiteral("DataTable")) {
        m_table = new QTableWidget(0, 2, m_canvas);
        m_table->setHorizontalHeaderLabels({ QStringLiteral("检测项目"), QStringLiteral("结果") });
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->verticalHeader()->setVisible(false);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setStyleSheet("QTableWidget { background:#f4f9ff; color:#24425f; gridline-color:#333; }"
                               "QHeaderView::section { background:#dcebf8; color:#4a6a8c; border:none; }");
        return m_table;
    }

    if (type == QStringLiteral("Button")) {
        // 绑定动作: run/start/stop; 未填则按文字猜测
        QString action = bind.trimmed();
        if (action.isEmpty()) {
            if (label.contains(QStringLiteral("停止"))) action = QStringLiteral("stop");
            else if (label.contains(QStringLiteral("连续")) || label.contains(QStringLiteral("启动")))
                action = QStringLiteral("start");
            else action = QStringLiteral("run");
        }
        auto* btn = new QPushButton(label, m_canvas);
        btn->setStyleSheet(
            "QPushButton { background-color:#2d5a88; color:#24425f; border:none;"
            " border-radius:6px; font-size:18px; font-weight:bold; }"
            "QPushButton:hover { background-color:#3a6da0; }"
            "QPushButton:pressed { background-color:#234668; }");
        const QString act = action;
        connect(btn, &QPushButton::clicked, this, [this, act]() { emit actionRequested(act); });
        return btn;
    }

    if (type == QStringLiteral("StatusPanel")) {
        m_statusPanel = new StatusPanel(m_canvas);
        m_statusPanel->setStationCount(8); // CCD1~8
        return m_statusPanel;
    }

    if (type == QStringLiteral("ValueDisplay")) {
        // 标题(小,灰) + 数值(大,黄); bind = "工具名.结果键"
        auto* container = new QWidget(m_canvas);
        container->setStyleSheet(
            "QWidget { background-color:#e4eef8; border:1px solid #2d4a6b; border-radius:4px; }"
            "QLabel { border:none; }");
        auto* vlay = new QVBoxLayout(container);
        vlay->setContentsMargins(6, 4, 6, 4);
        vlay->setSpacing(2);
        auto* title = new QLabel(bind.isEmpty() ? label : bind, container);
        title->setStyleSheet("color:#5a7a9c; font-size:12px;");
        title->setWordWrap(true);
        vlay->addWidget(title);
        auto* value = new QLabel(QStringLiteral("--"), container);
        value->setStyleSheet("color:#ffd400; font-weight:bold;");
        QFont f = value->font();
        f.setPixelSize(qBound(14, int(geo.height() * 0.42), 42));
        value->setFont(f);
        value->setAlignment(Qt::AlignCenter);
        vlay->addWidget(value, 1);
        // 绑定键: 支持 "$(工具.键)" 或裸 "工具.键"
        QString key = bind.trimmed();
        if (key.startsWith(QStringLiteral("$(")) && key.endsWith(QStringLiteral(")")))
            key = key.mid(2, key.size() - 3);
        if (!key.isEmpty())
            m_valueBinds.append({ value, key });
        return container;
    }

    // 未知类型: 显示占位标签
    auto* placeholder = new QLabel(label, m_canvas);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color:#9cc2e8; background:#222; border:1px dashed #b8d2ea;");
    return placeholder;
}

void RuntimeUI::setRuntimeImage(const QImage& img) {
    if (m_imageView) m_imageView->setImage(img);
}

void RuntimeUI::updateResults(const QList<QPair<QString, bool>>& toolStates,
                              const DataMap& resultData) {
    // 数据表: 工具/结果
    if (m_table) {
        m_table->setRowCount(toolStates.size());
        for (int r = 0; r < toolStates.size(); ++r) {
            m_table->setItem(r, 0, new QTableWidgetItem(toolStates[r].first));
            QTableWidgetItem* res = new QTableWidgetItem(toolStates[r].second ? "OK" : "NG");
            res->setForeground(QColor(toolStates[r].second ? 0x2e : 0xff,
                                      toolStates[r].second ? 0xc5 : 0x44, 0x2e));
            m_table->setItem(r, 1, res);
        }
    }
    // 数值显示: 按 绑定键 直查全局命名空间 "工具名.键"
    for (const ValueBind& vb : m_valueBinds) {
        if (!vb.label) continue;
        if (resultData.contains(vb.key)) {
            vb.label->setText(resultData.value(vb.key).toString());
        } else {
            vb.label->setText(QStringLiteral("--"));
        }
    }
}

void RuntimeUI::setStationStatus(int index, bool ok, const QString& name) {
    if (!m_statusPanel) return;
    m_statusPanel->setStationState(index, ok ? StatusIndicator::OK : StatusIndicator::NG);
    Q_UNUSED(name);
}

} // namespace VisionInspector
