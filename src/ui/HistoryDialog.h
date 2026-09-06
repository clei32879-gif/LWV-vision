/** @file HistoryDialog.h - 检测记录历史查询 (阶段6: 数据记录与报表)
 *
 *  从 SQLite records.db 按时间段/结果过滤查询, 支持导出CSV与按保留天数清理。
 */
#pragma once
#include <QDialog>
#include "../engine/DetectionRecorder.h"
class QTableWidget;
class QDateTimeEdit;
class QComboBox;
class QLabel;

namespace VisionInspector {

class HistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit HistoryDialog(DetectionRecorder* recorder, QWidget* parent = nullptr);

private:
    void doQuery();
    void doExport();
    void doPurge();

    DetectionRecorder* m_recorder;
    QDateTimeEdit* m_from = nullptr;
    QDateTimeEdit* m_to = nullptr;
    class QComboBox* m_resultCombo = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_summary = nullptr;
};

} // namespace VisionInspector
