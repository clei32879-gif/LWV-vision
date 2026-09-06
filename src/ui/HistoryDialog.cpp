/** @file HistoryDialog.cpp - 检测记录历史查询实现 */
#include "HistoryDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QStandardPaths>
#include <QColor>

namespace VisionInspector {

HistoryDialog::HistoryDialog(DetectionRecorder* recorder, QWidget* parent)
    : QDialog(parent), m_recorder(recorder) {
    setWindowTitle(QStringLiteral("检测记录 - 历史查询"));
    setMinimumSize(860, 560);

    auto* layout = new QVBoxLayout(this);

    // 过滤行
    auto* filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel(QStringLiteral("从"), this));
    m_from = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7), this);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_from->setCalendarPopup(true);
    filterRow->addWidget(m_from);
    filterRow->addWidget(new QLabel(QStringLiteral("到"), this));
    m_to = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_to->setCalendarPopup(true);
    filterRow->addWidget(m_to);
    m_resultCombo = new QComboBox(this);
    m_resultCombo->addItem(QStringLiteral("全部结果"));
    m_resultCombo->addItem(QStringLiteral("仅OK"));
    m_resultCombo->addItem(QStringLiteral("仅NG"));
    filterRow->addWidget(m_resultCombo);
    auto* queryBtn = new QPushButton(QStringLiteral("查询"), this);
    filterRow->addWidget(queryBtn);
    filterRow->addStretch(1);
    layout->addLayout(filterRow);

    // 记录表
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({ QStringLiteral("时间"), QStringLiteral("序号"),
                                         QStringLiteral("总结果"), QStringLiteral("项目判定"),
                                         QStringLiteral("数值") });
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);

    // 摘要 + 操作行
    m_summary = new QLabel(this);
    layout->addWidget(m_summary);
    auto* opRow = new QHBoxLayout();
    auto* exportBtn = new QPushButton(QStringLiteral("导出当前结果为CSV..."), this);
    auto* purgeBtn = new QPushButton(QStringLiteral("清理旧记录..."), this);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    opRow->addWidget(exportBtn);
    opRow->addWidget(purgeBtn);
    opRow->addStretch(1);
    opRow->addWidget(closeBtn);
    layout->addLayout(opRow);

    connect(queryBtn, &QPushButton::clicked, this, &HistoryDialog::doQuery);
    connect(exportBtn, &QPushButton::clicked, this, &HistoryDialog::doExport);
    connect(purgeBtn, &QPushButton::clicked, this, &HistoryDialog::doPurge);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    doQuery();
}

void HistoryDialog::doQuery() {
    if (!m_recorder || !m_recorder->isOpen()) {
        m_summary->setText(QStringLiteral("记录数据库未启用 (data/records.db 打开失败)"));
        m_table->setRowCount(0);
        return;
    }
    DetectionRecorder::QueryFilter f;
    f.from = m_from->dateTime();
    f.to = m_to->dateTime();
    f.resultMask = m_resultCombo->currentIndex() == 1 ? 0x2
                 : m_resultCombo->currentIndex() == 2 ? 0x1 : 0x3;
    const QList<InspectionRecord> rows = m_recorder->query(f);

    m_table->setRowCount(rows.size());
    int okCount = 0;
    for (int r = 0; r < rows.size(); ++r) {
        const InspectionRecord& rec = rows[r];
        if (rec.overallOk) ++okCount;
        m_table->setItem(r, 0, new QTableWidgetItem(
            rec.timestamp.toString(QStringLiteral("MM-dd HH:mm:ss"))));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::number(rec.index)));
        QTableWidgetItem* res = new QTableWidgetItem(rec.overallOk ? "OK" : "NG");
        res->setForeground(QColor(rec.overallOk ? 0x2e : 0xff, rec.overallOk ? 0xc5 : 0x44, 0x2e));
        m_table->setItem(r, 2, res);
        QStringList items;
        for (auto it = rec.itemResults.begin(); it != rec.itemResults.end(); ++it)
            items << QStringLiteral("%1:%2").arg(it.key(), it.value() ? "OK" : "NG");
        m_table->setItem(r, 3, new QTableWidgetItem(items.join(QStringLiteral("  "))));
        QStringList vals;
        for (auto it = rec.values.begin(); it != rec.values.end(); ++it)
            vals << QStringLiteral("%1=%2").arg(it.key()).arg(it.value());
        m_table->setItem(r, 4, new QTableWidgetItem(vals.join(QStringLiteral("  "))));
    }
    m_summary->setText(QStringLiteral("共 %1 条 (OK %2 / NG %3)%4")
        .arg(rows.size()).arg(okCount).arg(rows.size() - okCount)
        .arg(rows.size() == f.limit ? QStringLiteral("，已达单次查询上限") : QString()));
}

void HistoryDialog::doExport() {
    if (!m_recorder || !m_recorder->isOpen()) return;
    const QString def = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/检测记录_%1.csv")
              .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出检测记录"),
                                                      def, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty()) return;
    DetectionRecorder::QueryFilter f;
    f.from = m_from->dateTime();
    f.to = m_to->dateTime();
    f.resultMask = m_resultCombo->currentIndex() == 1 ? 0x2
                 : m_resultCombo->currentIndex() == 2 ? 0x1 : 0x3;
    f.limit = 1000000; // 导出不设上限
    QString err;
    const int n = m_recorder->exportCsv(path, f, &err);
    if (n >= 0)
        QMessageBox::information(this, QStringLiteral("导出检测记录"),
                                 QStringLiteral("已导出 %1 条到:\n%2").arg(n).arg(path));
    else
        QMessageBox::warning(this, QStringLiteral("导出检测记录"), err);
}

void HistoryDialog::doPurge() {
    if (!m_recorder || !m_recorder->isOpen()) return;
    bool ok = false;
    const int days = QInputDialog::getInt(this, QStringLiteral("清理旧记录"),
        QStringLiteral("删除保留天数之前的记录 (含当前时间往前推):"), 90, 1, 3650, 30, &ok);
    if (!ok) return;
    if (QMessageBox::question(this, QStringLiteral("清理旧记录"),
            QStringLiteral("确认删除 %1 天之前的全部检测记录? 此操作不可恢复。").arg(days))
        != QMessageBox::Yes)
        return;
    const int n = m_recorder->purgeBefore(days);
    QMessageBox::information(this, QStringLiteral("清理旧记录"),
                             QStringLiteral("已删除 %1 条记录。").arg(n));
    doQuery();
}

} // namespace VisionInspector
