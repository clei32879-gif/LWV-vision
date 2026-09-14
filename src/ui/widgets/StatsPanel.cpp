/**
 * @file StatsPanel.cpp
 * @brief 良率统计面板 — 自绘趋势图/NG分布 + CSV导出 (P0-5)
 */
#include "StatsPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QFileDialog>
#include <QMessageBox>

namespace VisionInspector {

StatsPanel::StatsPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void StatsPanel::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // 顶部: 大字良率 + 计数
    auto* topRow = new QHBoxLayout();
    m_yieldLabel = new QLabel(QStringLiteral("良率 --"), this);
    m_yieldLabel->setStyleSheet("font-size:28px; font-weight:bold; color:#00e676;");
    m_countLabel = new QLabel(QStringLiteral("总数 0 | 良 0 | NG 0 | 重测 0"), this);
    m_countLabel->setStyleSheet("font-size:13px; color:#3a5a7c;");
    m_exportBtn = new QPushButton(QStringLiteral("导出CSV"), this);
    topRow->addWidget(m_yieldLabel);
    topRow->addStretch();
    topRow->addWidget(m_countLabel);
    topRow->addWidget(m_exportBtn);
    root->addLayout(topRow);

    // 中部: 趋势图 + NG分布 (Label贴自绘Pixmap)
    m_trendLabel = new QLabel(this);
    m_trendLabel->setMinimumHeight(140);
    m_trendLabel->setAlignment(Qt::AlignCenter);
    m_trendLabel->setStyleSheet("background:#e4eef8; border:1px solid #333;");
    root->addWidget(m_trendLabel, 3);

    m_ngDistLabel = new QLabel(this);
    m_ngDistLabel->setMinimumHeight(110);
    m_ngDistLabel->setAlignment(Qt::AlignCenter);
    m_ngDistLabel->setStyleSheet("background:#e4eef8; border:1px solid #333;");
    root->addWidget(m_ngDistLabel, 2);

    connect(m_exportBtn, &QPushButton::clicked, this, &StatsPanel::onExportCsv);
}

void StatsPanel::setStats(GlobalStats* stats) {
    m_stats = stats;
    if (m_stats) {
        connect(m_stats, &GlobalStats::statsChanged,
                this, &StatsPanel::onStatsChanged);
        connect(m_stats, &GlobalStats::recordAdded,
                this, &StatsPanel::onStatsChanged);
        onStatsChanged();
    }
}

void StatsPanel::onStatsChanged() {
    if (!m_stats) return;
    const double yield = m_stats->yieldRate();
    m_yieldLabel->setText(QString::fromUtf8("良率 %1%").arg(yield, 0, 'f', 2));
    m_yieldLabel->setStyleSheet(yield >= 95
        ? "font-size:28px; font-weight:bold; color:#00e676;"
        : "font-size:28px; font-weight:bold; color:#ff5252;");
    m_countLabel->setText(QString::fromUtf8("总数 %1 | 良 %2 | NG %3 | 重测 %4")
        .arg(m_stats->totalCount()).arg(m_stats->passCount())
        .arg(m_stats->failCount()).arg(m_stats->retestCount()));
    drawTrend();
    drawNgDistribution();
}

void StatsPanel::drawTrend() {
    if (!m_trendLabel) return;
    const int W = m_trendLabel->width() > 20 ? m_trendLabel->width() : 560;
    const int H = m_trendLabel->height() > 20 ? m_trendLabel->height() : 140;
    QPixmap pm(W, H);
    pm.fill(QColor(0x14, 0x14, 0x14));
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // 最近120条记录的良率滚动窗口 (每10条一个采样点)
    const auto& recs = m_stats->records();
    QVector<QPair<int, double>> points;   // (序号, 滚动良率%)
    const int win = 10;
    for (int i = 0; i < recs.size(); i += win) {
        int ok = 0, n = 0;
        for (int j = i; j < qMin(i + win, recs.size()); ++j) {
            if (recs[j].overallOk) ++ok;
            ++n;
        }
        if (n > 0) points.append({i + n, 100.0 * ok / n});
    }

    // 网格
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1));
    for (int i = 0; i <= 4; ++i) {
        const int y = H * i / 4;
        p.drawLine(40, y, W - 10, y);
    }
    // 100%/50%标线
    p.setPen(QPen(QColor(0x55, 0x55, 0x55), 1, Qt::DashLine));
    p.drawLine(40, 4, W - 10, 4);
    p.drawLine(40, H / 2, W - 10, H / 2);
    p.setPen(QColor(0x88, 0x88, 0x88));
    p.drawText(2, 14, "100%");
    p.drawText(2, H / 2 + 4, "50%");
    p.drawText(2, H - 4, "0%");

    if (points.size() >= 2) {
        // 折线
        QPen linePen(QColor(0x00, 0xE6, 0x76), 2);
        p.setPen(linePen);
        const double dx = double(W - 50) / qMax(1, points.last().first);
        for (int i = 1; i < points.size(); ++i) {
            const int x0 = 40 + int(points[i-1].first * dx);
            const int x1 = 40 + int(points[i].first * dx);
            const int y0 = H - int(H * points[i-1].second / 100.0);
            const int y1 = H - int(H * points[i].second / 100.0);
            p.drawLine(x0, y0, x1, y1);
        }
        // 最新点标注
        const auto& last = points.last();
        p.setBrush(QColor(0x00, 0xE6, 0x76));
        p.drawEllipse(40 + int(last.first * dx) - 3,
                      H - int(H * last.second / 100.0) - 3, 6, 6);
    } else {
        p.setPen(QColor(0x66, 0x66, 0x66));
        p.drawText(pm.rect(), Qt::AlignCenter,
                   QString::fromUtf8("良率趋势 (运行后显示, 窗口%1件)").arg(win));
    }
    m_trendLabel->setPixmap(pm);
}

void StatsPanel::drawNgDistribution() {
    if (!m_ngDistLabel) return;
    const int W = m_ngDistLabel->width() > 20 ? m_ngDistLabel->width() : 560;
    const int H = m_ngDistLabel->height() > 20 ? m_ngDistLabel->height() : 110;
    QPixmap pm(W, H);
    pm.fill(QColor(0x14, 0x14, 0x14));
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // 按检测项统计NG数 (record.itemResults: 项目名→是否通过)
    QMap<QString, int> ngByItem;
    for (const auto& rec : m_stats->records()) {
        if (rec.overallOk) continue;
        for (auto it = rec.itemResults.begin(); it != rec.itemResults.end(); ++it)
            if (!it.value()) ++ngByItem[it.key()];
    }

    if (ngByItem.isEmpty()) {
        p.setPen(QColor(0x66, 0x66, 0x66));
        p.drawText(pm.rect(), Qt::AlignCenter,
                   QString::fromUtf8("NG 分布 (检测出NG后显示各检测项占比)"));
        m_ngDistLabel->setPixmap(pm);
        return;
    }

    // 水平条形图
    const int barH = 22;
    int maxNg = 1;
    for (auto it = ngByItem.begin(); it != ngByItem.end(); ++it)
        maxNg = qMax(maxNg, it.value());
    int y = 6;
    const QStringList colors = {"#ff5252", "#ff9800", "#ffca28", "#ab47bc", "#26c6da"};
    int ci = 0;
    for (auto it = ngByItem.begin(); it != ngByItem.end() && y + barH < H; ++it, ++ci) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(colors[ci % colors.size()]));
        const int bw = int((W - 260) * it.value() / maxNg);
        p.drawRect(170, y, qMax(4, bw), barH - 4);
        p.setPen(QColor(0xdd, 0xdd, 0xdd));
        p.drawText(QRect(0, y, 165, barH), Qt::AlignRight | Qt::AlignVCenter,
                   it.key());
        p.drawText(180 + qMax(4, bw), y + barH - 8, QString::number(it.value()));
        y += barH + 2;
    }
    m_ngDistLabel->setPixmap(pm);
}

void StatsPanel::onExportCsv() {
    if (!m_stats || m_stats->historyCount() == 0) {
        QMessageBox::information(this, QString::fromUtf8("导出"),
                                 QString::fromUtf8("暂无检测记录"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QString::fromUtf8("导出检测记录CSV"),
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_records.csv",
        "CSV (*.csv)");
    if (path.isEmpty()) return;
    QString err;
    if (m_stats->exportCsv(path, &err))
        QMessageBox::information(this, QString::fromUtf8("导出完成"),
            QString::fromUtf8("已导出 %1 条记录").arg(m_stats->historyCount()));
    else
        QMessageBox::warning(this, QString::fromUtf8("导出失败"), err);
}

} // namespace VisionInspector
