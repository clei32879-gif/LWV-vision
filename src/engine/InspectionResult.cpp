/**
 * @file InspectionResult.cpp
 * @brief 检测结果实现
 */

#include "InspectionResult.h"

#include <QFile>
#include <QTextStream>
#include <QStringConverter>

namespace VisionInspector {

// ============================================================
// ItemStats
// ============================================================

QJsonObject ItemStats::toJson() const {
    QJsonObject json;
    json["name"] = name;
    json["upperLimit"] = upperLimit;
    json["lowerLimit"] = lowerLimit;
    json["okCount"] = okCount;
    json["ngCount"] = ngCount;
    return json;
}

void ItemStats::fromJson(const QJsonObject& json) {
    name = json.value("name").toString();
    upperLimit = json.value("upperLimit").toDouble();
    lowerLimit = json.value("lowerLimit").toDouble();
    okCount = json.value("okCount").toInt();
    ngCount = json.value("ngCount").toInt();
}

// ============================================================
// InspectionRecord
// ============================================================

QJsonObject InspectionRecord::toJson() const {
    QJsonObject json;
    json["index"] = index;
    json["timestamp"] = timestamp.toString(Qt::ISODate);
    json["overallOk"] = overallOk;
    json["isRetest"] = isRetest;

    QJsonObject valuesJson;
    for (auto it = values.begin(); it != values.end(); ++it) {
        valuesJson[it.key()] = it.value();
    }
    json["values"] = valuesJson;

    QJsonObject resultsJson;
    for (auto it = itemResults.begin(); it != itemResults.end(); ++it) {
        resultsJson[it.key()] = it.value();
    }
    json["itemResults"] = resultsJson;

    return json;
}

void InspectionRecord::fromJson(const QJsonObject& json) {
    index = json.value("index").toInt();
    timestamp = QDateTime::fromString(json.value("timestamp").toString(), Qt::ISODate);
    overallOk = json.value("overallOk").toBool();
    isRetest = json.value("isRetest").toBool();

    QJsonObject valuesJson = json.value("values").toObject();
    for (auto it = valuesJson.begin(); it != valuesJson.end(); ++it) {
        values[it.key()] = it.value().toDouble();
    }

    QJsonObject resultsJson = json.value("itemResults").toObject();
    for (auto it = resultsJson.begin(); it != resultsJson.end(); ++it) {
        itemResults[it.key()] = it.value().toBool();
    }
}

// ============================================================
// GlobalStats
// ============================================================

GlobalStats::GlobalStats(QObject* parent)
    : QObject(parent)
{
}

ItemStats GlobalStats::itemStats(const QString& name) const {
    for (const auto& stats : m_itemStats) {
        if (stats.name == name)
            return stats;
    }
    return ItemStats{name, 0, 0, 0, 0};
}

void GlobalStats::addRecord(const InspectionRecord& record) {
    m_totalCount++;

    if (record.isRetest) {
        m_retestCount++;
    }

    if (record.overallOk) {
        m_passCount++;
    } else {
        m_failCount++;
    }

    // 更新各检测项统计
    for (auto it = record.itemResults.begin(); it != record.itemResults.end(); ++it) {
        // 查找或创建项统计
        bool found = false;
        for (auto& stats : m_itemStats) {
            if (stats.name == it.key()) {
                if (it.value())
                    stats.okCount++;
                else
                    stats.ngCount++;
                found = true;
                break;
            }
        }
        if (!found) {
            ItemStats newStats;
            newStats.name = it.key();
            if (it.value())
                newStats.okCount = 1;
            else
                newStats.ngCount = 1;
            m_itemStats.append(newStats);
        }
    }

    // 追加历史记录 (有界)
    m_history.append(record);
    while (m_history.size() > kMaxHistory)
        m_history.removeFirst();

    emit recordAdded(record);
    emit statsChanged();
}

// CSV 字段转义 (含逗号/引号/换行时用双引号包裹)
static QString csvField(const QString& v) {
    if (v.contains(',') || v.contains('"') || v.contains('\n')) {
        QString s = v;
        s.replace('"', "\"\"");
        return '"' + s + '"';
    }
    return v;
}

bool GlobalStats::exportCsv(const QString& path, QString* err) const {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString("无法打开文件: %1").arg(path);
        return false;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts.setGenerateByteOrderMark(true);   // UTF-8 BOM, Excel 识别中文

    // 表头: 序号,时间,结果,<各检测项>...,数值明细
    QStringList header;
    header << QStringLiteral("序号") << QStringLiteral("时间") << QStringLiteral("结果");
    for (const auto& s : m_itemStats)
        header << csvField(s.name);
    header << QStringLiteral("数值明细");
    ts << header.join(',') << "\n";

    for (const auto& rec : m_history) {
        QStringList row;
        row << QString::number(rec.index)
            << rec.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
            << (rec.overallOk ? QStringLiteral("OK") : QStringLiteral("NG"));
        for (const auto& s : m_itemStats) {
            if (rec.itemResults.contains(s.name))
                row << (rec.itemResults.value(s.name) ? QStringLiteral("OK") : QStringLiteral("NG"));
            else
                row << QString();
        }
        QStringList details;
        for (auto it = rec.values.begin(); it != rec.values.end(); ++it)
            details << QStringLiteral("%1=%2").arg(it.key()).arg(it.value());
        row << csvField(details.join(';'));
        ts << row.join(',') << "\n";
    }
    ts.flush();
    f.close();
    return true;
}

void GlobalStats::setItemConfig(const QString& name, double upper, double lower) {
    for (auto& stats : m_itemStats) {
        if (stats.name == name) {
            stats.upperLimit = upper;
            stats.lowerLimit = lower;
            return;
        }
    }
    // 不存在则创建
    ItemStats stats;
    stats.name = name;
    stats.upperLimit = upper;
    stats.lowerLimit = lower;
    m_itemStats.append(stats);
}

void GlobalStats::clear() {
    m_totalCount = 0;
    m_passCount = 0;
    m_failCount = 0;
    m_retestCount = 0;
    m_itemStats.clear();
    m_history.clear();
    emit statsChanged();
}

QJsonObject GlobalStats::toJson() const {
    QJsonObject json;
    json["totalCount"] = m_totalCount;
    json["passCount"] = m_passCount;
    json["failCount"] = m_failCount;
    json["retestCount"] = m_retestCount;

    QJsonArray itemsArray;
    for (const auto& stats : m_itemStats) {
        itemsArray.append(stats.toJson());
    }
    json["itemStats"] = itemsArray;

    return json;
}

void GlobalStats::fromJson(const QJsonObject& json) {
    m_totalCount = json.value("totalCount").toInt();
    m_passCount = json.value("passCount").toInt();
    m_failCount = json.value("failCount").toInt();
    m_retestCount = json.value("retestCount").toInt();

    m_itemStats.clear();
    QJsonArray itemsArray = json.value("itemStats").toArray();
    for (const auto& item : itemsArray) {
        ItemStats stats;
        stats.fromJson(item.toObject());
        m_itemStats.append(stats);
    }

    emit statsChanged();
}

} // namespace VisionInspector
