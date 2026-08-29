#pragma once

#include "../utils/Common.h"
#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QVariant>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

namespace VisionInspector {

struct ItemStats {
    QString name;
    double upperLimit = 0;
    double lowerLimit = 0;
    int okCount = 0;
    int ngCount = 0;

    int totalCount() const { return okCount + ngCount; }
    double okRate() const { return totalCount() > 0 ? (double)okCount / totalCount() * 100.0 : 0.0; }
    double ngRate() const { return totalCount() > 0 ? (double)ngCount / totalCount() * 100.0 : 0.0; }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};

struct InspectionRecord {
    int index;
    QDateTime timestamp;
    bool overallOk;
    bool isRetest;
    QMap<QString, double> values;
    QMap<QString, bool> itemResults;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};

class GlobalStats : public QObject {
    Q_OBJECT

public:
    explicit GlobalStats(QObject* parent = nullptr);

    int totalCount() const { return m_totalCount; }
    int passCount() const { return m_passCount; }
    int failCount() const { return m_failCount; }
    int retestCount() const { return m_retestCount; }

    double yieldRate() const {
        return m_totalCount > 0 ? (double)m_passCount / m_totalCount * 100.0 : 0.0;
    }

    QList<ItemStats> itemStats() const { return m_itemStats; }
    ItemStats itemStats(const QString& name) const;

    void addRecord(const InspectionRecord& record);
    void setItemConfig(const QString& name, double upper, double lower);
    void clear();

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

signals:
    void statsChanged();
    void recordAdded(const InspectionRecord& record);

private:
    int m_totalCount = 0;
    int m_passCount = 0;
    int m_failCount = 0;
    int m_retestCount = 0;
    QList<ItemStats> m_itemStats;
};

} // namespace VisionInspector
