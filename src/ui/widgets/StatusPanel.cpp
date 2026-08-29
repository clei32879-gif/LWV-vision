#include "StatusPanel.h"
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFont>

namespace VisionInspector {

// ============================================================
// StatusIndicator
// ============================================================

StatusIndicator::StatusIndicator(const QString& name, QWidget* parent)
    : QWidget(parent), m_name(name) {
    setFixedSize(80, 60);
}

void StatusIndicator::setState(State state) {
    m_state = state;
    update();
}

void StatusIndicator::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 背景
    QColor bgColor;
    switch (m_state) {
    case OK:    bgColor = QColor(0, 180, 0); break;
    case NG:    bgColor = QColor(220, 30, 30); break;
    case Pause: bgColor = QColor(220, 180, 0); break;
    case Error: bgColor = QColor(100, 100, 100); break;
    default:    bgColor = QColor(60, 60, 60); break;
    }
    p.setBrush(bgColor);
    p.setPen(QPen(QColor(40, 40, 40), 2));
    p.drawRoundedRect(2, 2, width() - 4, height() - 4, 6, 6);

    // 状态文字
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(14);
    p.setFont(f);
    p.setPen(Qt::white);
    QString text;
    switch (m_state) {
    case OK:    text = "OK"; break;
    case NG:    text = "NG"; break;
    case Pause: text = "---"; break;
    case Error: text = "ERR"; break;
    default:    text = "---"; break;
    }
    p.drawText(rect().adjusted(0, 0, 0, -10), Qt::AlignCenter, text);

    // 名称
    f.setPointSize(8);
    p.setFont(f);
    p.setPen(QColor(180, 180, 180));
    p.drawText(rect().adjusted(0, height() - 18, 0, 0), Qt::AlignCenter, m_name);
}

// ============================================================
// StatusPanel
// ============================================================

StatusPanel::StatusPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_gridLayout = new QGridLayout();
    m_gridLayout->setSpacing(4);
    layout->addLayout(m_gridLayout);

    m_summaryLabel = new QLabel("OK: 0  NG: 0", this);
    m_summaryLabel->setStyleSheet("color: #ddd; font-size: 12px; padding: 4px;");
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_summaryLabel);
}

void StatusPanel::setStationCount(int count) {
    // 清除旧的
    for (auto* ind : m_indicators) {
        m_gridLayout->removeWidget(ind);
        ind->deleteLater();
    }
    m_indicators.clear();

    // 创建新的
    int cols = (count <= 4) ? count : 4;
    for (int i = 0; i < count; ++i) {
        auto* ind = new StatusIndicator(QString("CCD%1").arg(i + 1), this);
        m_indicators.append(ind);
        int row = i / cols;
        int col = i % cols;
        m_gridLayout->addWidget(ind, row, col);

        // 点击信号
        int idx = i;
        connect(ind, &QWidget::customContextMenuRequested, this, [this, idx]() {
            emit stationClicked(idx);
        });
        ind->setContextMenuPolicy(Qt::CustomContextMenu);
    }
}

void StatusPanel::setStationState(int index, StatusIndicator::State state) {
    if (index < 0 || index >= m_indicators.size()) return;
    StatusIndicator::State oldState = m_indicators[index]->state();
    m_indicators[index]->setState(state);

    // 更新计数
    if (oldState == StatusIndicator::OK && state != StatusIndicator::OK) m_okCount--;
    if (oldState == StatusIndicator::NG && state != StatusIndicator::NG) m_ngCount--;
    if (state == StatusIndicator::OK) m_okCount++;
    if (state == StatusIndicator::NG) m_ngCount++;

    m_summaryLabel->setText(QString("OK: %1  NG: %2").arg(m_okCount).arg(m_ngCount));
}

StatusIndicator::State StatusPanel::stationState(int index) const {
    if (index < 0 || index >= m_indicators.size()) return StatusIndicator::Idle;
    return m_indicators[index]->state();
}

int StatusPanel::okCount() const { return m_okCount; }
int StatusPanel::ngCount() const { return m_ngCount; }

void StatusPanel::resetCounts() {
    m_okCount = 0;
    m_ngCount = 0;
    for (auto* ind : m_indicators) {
        ind->setState(StatusIndicator::Idle);
    }
    m_summaryLabel->setText("OK: 0  NG: 0");
}

} // namespace VisionInspector
