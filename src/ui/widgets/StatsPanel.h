/** @file StatsPanel.h - 良率统计面板: 实时良率 + 趋势图(QPainter自绘) + NG分布 + CSV导出 */
#pragma once
#include "../engine/InspectionResult.h"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVector>

namespace VisionInspector {

class StatsPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatsPanel(QWidget* parent = nullptr);
    void setStats(GlobalStats* stats);

private slots:
    void onStatsChanged();
    void onExportCsv();

private:
    void setupUi();
    void drawTrend();           // 良率趋势(自绘QPainter)
    void drawNgDistribution();  // NG类别分布(自绘)

    GlobalStats* m_stats = nullptr;
    QLabel* m_yieldLabel = nullptr;      // 大字良率
    QLabel* m_countLabel = nullptr;      // 总数/良/NG/重测
    QLabel* m_trendLabel = nullptr;      // 趋势图(绘制到Pixmap贴Label)
    QLabel* m_ngDistLabel = nullptr;     // NG分布
    QPushButton* m_exportBtn = nullptr;
};

} // namespace VisionInspector
