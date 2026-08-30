#include "DataPanel.h"
#include "../core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHeaderView>

namespace VisionInspector {

DataPanel::DataPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({
        "CCD", "检测项目", "当前值", "下限", "上限", "良率", "下限NG", "上限NG"
    });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    // 不用交替行颜色，手动统一背景
    m_table->setAlternatingRowColors(false);

    const QStringList stations = ConfigManager::instance().stationNames();
    m_table->setRowCount(stations.size());
    for (int i = 0; i < stations.size(); ++i) {
        for (int j = 0; j < 8; ++j) {
            auto* item = new QTableWidgetItem(j == 0 ? stations.at(i) : "--");
            item->setTextAlignment(Qt::AlignCenter);
            // 统一深色背景 + 浅色文字，不分单双行
            item->setBackground(QColor(30, 30, 30));
            item->setForeground(QColor(224, 224, 224));
            m_table->setItem(i, j, item);
        }
    }

    // 列宽：全部固定，不用Stretch
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->setColumnWidth(0, 60);   // CCD
    m_table->setColumnWidth(1, 100);  // 检测项目
    m_table->setColumnWidth(2, 80);   // 当前值
    m_table->setColumnWidth(3, 70);   // 下限
    m_table->setColumnWidth(4, 70);   // 上限
    m_table->setColumnWidth(5, 60);   // 良率
    m_table->setColumnWidth(6, 70);   // 下限NG
    m_table->setColumnWidth(7, 70);   // 上限NG

    // 固定行高
    m_table->verticalHeader()->setDefaultSectionSize(28);

    layout->addWidget(m_table);
}

void DataPanel::setStats(GlobalStats* stats) {
    m_stats = stats;
    if (m_stats) {
        connect(m_stats, &GlobalStats::statsChanged, this, &DataPanel::onStatsChanged);
        onStatsChanged();
    }
}

void DataPanel::updateResult(int ccdIndex, const QString& item, double value, double lower, double upper) {
    if (ccdIndex < 0 || ccdIndex >= m_table->rowCount()) return;
    int row = ccdIndex;
    m_table->item(row, 1)->setText(item);
    m_table->item(row, 2)->setText(QString::number(value, 'f', 3));
    m_table->item(row, 3)->setText(QString::number(lower, 'f', 3));
    m_table->item(row, 4)->setText(QString::number(upper, 'f', 3));

    bool ok = (value >= lower && value <= upper);
    m_table->item(row, 5)->setText(ok ? "100%" : "0%");
    m_table->item(row, 5)->setForeground(ok ? QColor(0, 200, 0) : QColor(255, 80, 80));
    m_table->item(row, 6)->setText(ok ? "--" : "超下限");
    m_table->item(row, 7)->setText(ok ? "--" : "超上限");
}

void DataPanel::setStationStatus(int index, bool ok, const QString& detail) {
    if (index < 0 || index >= m_table->rowCount()) return;
    if (!detail.isEmpty()) {
        m_table->item(index, 1)->setText(detail);
    }
    m_table->item(index, 5)->setText(ok ? "100%" : "0%");
    m_table->item(index, 5)->setForeground(ok ? QColor(0, 200, 0) : QColor(255, 80, 80));
}

void DataPanel::onStatsChanged() {
    if (!m_stats || !m_summaryLabel) return;
    int total = m_stats->totalCount();
    int pass = m_stats->passCount();
    int fail = m_stats->failCount();
    double yieldRate = (total > 0) ? (double)pass / total * 100.0 : 100.0;
    m_summaryLabel->setText(QString("OK: %1  NG: %2  总数: %3  良率: %4%")
        .arg(pass).arg(fail).arg(total).arg(QString::number(yieldRate, 'f', 1)));
}

} // namespace VisionInspector
