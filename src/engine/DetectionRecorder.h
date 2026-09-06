/** @file DetectionRecorder.h - 检测记录 SQLite 持久化 (阶段6: 数据记录与报表)
 *
 *  每次执行结果落库 (exe旁 data/records.db), 关机不丢;
 *  支持按时间段/结果过滤查询、按当前过滤导出CSV、按保留天数清理。
 *  无 Qt6::Sql 时编译为空实现 (open 恒 false, 其余调用安全无操作)。
 */
#pragma once
#include "../engine/InspectionResult.h"
#include <QObject>
#include <QDateTime>
#include <QList>

class QSqlDatabase;

namespace VisionInspector {

class DetectionRecorder : public QObject {
    Q_OBJECT
public:
    explicit DetectionRecorder(QObject* parent = nullptr);
    ~DetectionRecorder() override;

    struct QueryFilter {
        QDateTime from;        // 空=不限
        QDateTime to;
        int resultMask = 0x3;  // bit0=含NG bit1=含OK; 1=仅NG 2=仅OK 3=全部
        int limit = 2000;      // 最多返回条数 (按时间倒序取最新)
    };

    /** 打开数据库 (默认 exe旁 data/records.db) 并建表; 失败返回false */
    bool open(const QString& dbPath = {});
    void close();
    bool isOpen() const;

    /** 写入一条检测记录 */
    void record(const InspectionRecord& rec);

    /** 按过滤条件查询 (时间倒序) */
    QList<InspectionRecord> query(const QueryFilter& f) const;

    /** 按过滤条件导出CSV (UTF-8 BOM); 返回导出行数, 失败返回-1 */
    int exportCsv(const QString& path, const QueryFilter& f, QString* err = nullptr) const;

    /** 删除保留天数之前的记录; 返回删除行数 (keepDays<=0 不清理) */
    int purgeBefore(int keepDays);

    qint64 totalCount() const;

private:
    QSqlDatabase* m_db = nullptr;   // PIMPL避免头文件引入QSqlDatabase
    QString m_connName;
};

} // namespace VisionInspector
