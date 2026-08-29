/** @file DataPanel.h - CCD检测项目表 */
#pragma once
#include "../engine/InspectionResult.h"
#include <QWidget>
#include <QTableWidget>
#include <QLabel>

namespace VisionInspector {

class DataPanel : public QWidget {
    Q_OBJECT
public:
    explicit DataPanel(QWidget* parent = nullptr);
    void setStats(GlobalStats* stats);
    void updateResult(int ccdIndex, const QString& item, double value, double lower, double upper);
    void setStationStatus(int index, bool ok, const QString& detail = QString());

private slots:
    void onStatsChanged();

private:
    QTableWidget* m_table;
    QLabel* m_summaryLabel;
    GlobalStats* m_stats = nullptr;
};

} // namespace VisionInspector
