/**
 * @file InspectionResult.cpp
 * @brief 检测结果实现
 */

#include "InspectionResult.h"

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

    emit recordAdded(record);
    emit statsChanged();
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
