/** @file DetectionRecorder.cpp - 检测记录 SQLite 持久化实现 */
#include "DetectionRecorder.h"

#ifdef VI_HAS_QTSQL
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

namespace VisionInspector {

DetectionRecorder::DetectionRecorder(QObject* parent)
    : QObject(parent) {}

DetectionRecorder::~DetectionRecorder() { close(); }

bool DetectionRecorder::open(const QString& dbPath) {
    close();
    const QString path = dbPath.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/data/records.db")
        : dbPath;
    QDir().mkpath(QFileInfo(path).absolutePath());

    m_connName = QStringLiteral("lwvision_records_%1").arg(
        QDateTime::currentMSecsSinceEpoch());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connName);
    db.setDatabaseName(path);
    if (!db.open()) return false;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS records ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " ts TEXT NOT NULL,"
            " ok INTEGER NOT NULL,"
            " retest INTEGER NOT NULL DEFAULT 0,"
            " items TEXT,"
            " vals TEXT)"))) return false;
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_records_ts ON records(ts)"));

    m_db = new QSqlDatabase(db);
    return true;
}

void DetectionRecorder::close() {
    if (m_db) {
        m_db->close();
        delete m_db;
        m_db = nullptr;
        const QString conn = m_connName;
        m_connName.clear();
        QSqlDatabase::removeDatabase(conn);
    }
}

bool DetectionRecorder::isOpen() const { return m_db != nullptr; }

void DetectionRecorder::record(const InspectionRecord& rec) {
    if (!m_db) return;
    QJsonObject items, vals;
    for (auto it = rec.itemResults.begin(); it != rec.itemResults.end(); ++it)
        items[it.key()] = it.value();
    for (auto it = rec.values.begin(); it != rec.values.end(); ++it)
        vals[it.key()] = QJsonValue::fromVariant(it.value());

    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO records (ts, ok, retest, items, vals) VALUES (?,?,?,?,?)"));
    q.addBindValue(rec.timestamp.toString(Qt::ISODate));
    q.addBindValue(rec.overallOk ? 1 : 0);
    q.addBindValue(rec.isRetest ? 1 : 0);
    q.addBindValue(QString::fromUtf8(QJsonDocument(items).toJson(QJsonDocument::Compact)));
    q.addBindValue(QString::fromUtf8(QJsonDocument(vals).toJson(QJsonDocument::Compact)));
    q.exec(); // 单条失败不中断产线, 由 totalCount/query 侧反映
}

QList<InspectionRecord> DetectionRecorder::query(const QueryFilter& f) const {
    QList<InspectionRecord> out;
    if (!m_db) return out;

    QStringList cond;
    if (f.from.isValid()) cond << QStringLiteral("ts >= '%1'").arg(f.from.toString(Qt::ISODate));
    if (f.to.isValid()) cond << QStringLiteral("ts <= '%1'").arg(f.to.toString(Qt::ISODate));
    if (f.resultMask == 0x1) cond << QStringLiteral("ok = 0");
    else if (f.resultMask == 0x2) cond << QStringLiteral("ok = 1");

    QSqlQuery q(*m_db);
    QString sql = QStringLiteral("SELECT id, ts, ok, retest, items, vals FROM records");
    if (!cond.isEmpty()) sql += QStringLiteral(" WHERE ") + cond.join(QStringLiteral(" AND "));
    sql += QStringLiteral(" ORDER BY id DESC LIMIT %1").arg(f.limit);
    if (!q.exec(sql)) return out;

    while (q.next()) {
        InspectionRecord rec;
        rec.index = q.value(0).toInt();
        rec.timestamp = QDateTime::fromString(q.value(1).toString(), Qt::ISODate);
        rec.overallOk = q.value(2).toInt() != 0;
        rec.isRetest = q.value(3).toInt() != 0;
        const QJsonObject items = QJsonDocument::fromJson(q.value(4).toString().toUtf8()).object();
        for (auto it = items.begin(); it != items.end(); ++it)
            rec.itemResults[it.key()] = it.value().toBool();
        const QJsonObject vals = QJsonDocument::fromJson(q.value(5).toString().toUtf8()).object();
        for (auto it = vals.begin(); it != vals.end(); ++it)
            rec.values[it.key()] = it.value().toDouble();
        out.append(rec);
    }
    return out;
}

int DetectionRecorder::exportCsv(const QString& path, const QueryFilter& f, QString* err) const {
    const QList<InspectionRecord> rows = query(f);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法写入文件: %1").arg(path);
        return -1;
    }
    file.write("\xEF\xBB\xBF"); // UTF-8 BOM, Excel 直开
    file.write("序号,时间,总结果,项目判定,数值\n");
    for (const InspectionRecord& r : rows) {
        QStringList items;
        for (auto it = r.itemResults.begin(); it != r.itemResults.end(); ++it)
            items << QStringLiteral("%1=%2").arg(it.key(), it.value() ? "OK" : "NG");
        QStringList vals;
        for (auto it = r.values.begin(); it != r.values.end(); ++it)
            vals << QStringLiteral("%1=%2").arg(it.key()).arg(it.value());
        auto esc = [](const QString& s) {
            return s.contains(',') || s.contains('"') || s.contains('\n')
                       ? QStringLiteral("\"%1\"").arg(QString(s).replace('"', "\"\""))
                       : s;
        };
        const QString line = QStringLiteral("%1,%2,%3,%4,%5\n")
            .arg(r.index)
            .arg(r.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
            .arg(r.overallOk ? "OK" : "NG")
            .arg(esc(items.join(QStringLiteral("; "))))
            .arg(esc(vals.join(QStringLiteral("; "))));
        file.write(line.toUtf8());
    }
    return rows.size();
}

int DetectionRecorder::purgeBefore(int keepDays) {
    if (!m_db || keepDays <= 0) return 0;
    const QString cutoff = QDateTime::currentDateTime().addDays(-keepDays).toString(Qt::ISODate);
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("DELETE FROM records WHERE ts < ?"));
    q.addBindValue(cutoff);
    if (!q.exec()) return -1;
    return q.numRowsAffected();
}

qint64 DetectionRecorder::totalCount() const {
    if (!m_db) return 0;
    QSqlQuery q(*m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM records")) || !q.next()) return 0;
    return q.value(0).toLongLong();
}

} // namespace VisionInspector

#else // !VI_HAS_QTSQL — 空实现, 保证无Sql模块时整体可编译

namespace VisionInspector {
DetectionRecorder::DetectionRecorder(QObject* parent) : QObject(parent) {}
DetectionRecorder::~DetectionRecorder() = default;
bool DetectionRecorder::open(const QString&) { return false; }
void DetectionRecorder::close() {}
bool DetectionRecorder::isOpen() const { return false; }
void DetectionRecorder::record(const InspectionRecord&) {}
QList<InspectionRecord> DetectionRecorder::query(const QueryFilter&) const { return {}; }
int DetectionRecorder::exportCsv(const QString&, const QueryFilter&, QString* err) const {
    if (err) *err = QStringLiteral("Qt Sql 模块未启用");
    return -1;
}
int DetectionRecorder::purgeBefore(int) { return 0; }
qint64 DetectionRecorder::totalCount() const { return 0; }
} // namespace VisionInspector

#endif // VI_HAS_QTSQL
