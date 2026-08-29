#include "DataJudgment.h"

namespace VisionInspector {

void DataJudgmentManager::addItem(const QString& name, const QString& dataKey,
                                   double lower, double upper, bool enabled) {
    DataJudgmentItem item;
    item.name = name;
    item.dataKey = dataKey;
    item.lowerLimit = lower;
    item.upperLimit = upper;
    item.enabled = enabled;
    m_items.append(item);
}

void DataJudgmentManager::removeItem(const QString& name) {
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (m_items[i].name == name) {
            m_items.removeAt(i);
            break;
        }
    }
}

void DataJudgmentManager::updateItem(const QString& name, double lower, double upper, bool enabled) {
    for (auto& item : m_items) {
        if (item.name == name) {
            item.lowerLimit = lower;
            item.upperLimit = upper;
            item.enabled = enabled;
            break;
        }
    }
}

bool DataJudgmentManager::judge(const QMap<QString, QVariant>& data) {
    bool allOk = true;
    bool hasEnabled = false;
    
    for (auto& item : m_items) {
        if (!item.enabled) {
            item.result = JudgmentResult::NoLimit;
            continue;
        }
        
        hasEnabled = true;
        
        if (!data.contains(item.dataKey)) {
            item.result = JudgmentResult::Invalid;
            allOk = false;
            continue;
        }
        
        item.value = data[item.dataKey].toDouble();
        
        if (item.value >= item.lowerLimit && item.value <= item.upperLimit) {
            item.result = JudgmentResult::OK;
        } else {
            item.result = JudgmentResult::NG;
            allOk = false;
        }
    }
    
    if (!hasEnabled) {
        m_overallResult = JudgmentResult::NoLimit;
    } else if (allOk) {
        m_overallResult = JudgmentResult::OK;
    } else {
        m_overallResult = JudgmentResult::NG;
    }
    
    return allOk;
}

QString DataJudgmentManager::overallResultString() const {
    switch (m_overallResult) {
    case JudgmentResult::OK: return "OK";
    case JudgmentResult::NG: return "NG";
    case JudgmentResult::NoLimit: return "未设置";
    case JudgmentResult::Invalid: return "无效";
    default: return "未知";
    }
}

DataJudgmentManager DataJudgmentManager::fromProperties(const QMap<QString, QVariant>& props) {
    DataJudgmentManager mgr;
    int count = props.value("judgmentCount", 0).toInt();
    for (int i = 0; i < count; ++i) {
        QString prefix = QString("judgment_%1_").arg(i);
        DataJudgmentItem item;
        item.name = props.value(prefix + "name").toString();
        item.dataKey = props.value(prefix + "dataKey").toString();
        item.lowerLimit = props.value(prefix + "lower", -99999.0).toDouble();
        item.upperLimit = props.value(prefix + "upper", 99999.0).toDouble();
        item.enabled = props.value(prefix + "enabled", false).toBool();
        mgr.m_items.append(item);
    }
    return mgr;
}

QMap<QString, QVariant> DataJudgmentManager::toProperties() const {
    QMap<QString, QVariant> props;
    props["judgmentCount"] = m_items.size();
    for (int i = 0; i < m_items.size(); ++i) {
        QString prefix = QString("judgment_%1_").arg(i);
        props[prefix + "name"] = m_items[i].name;
        props[prefix + "dataKey"] = m_items[i].dataKey;
        props[prefix + "lower"] = m_items[i].lowerLimit;
        props[prefix + "upper"] = m_items[i].upperLimit;
        props[prefix + "enabled"] = m_items[i].enabled;
    }
    return props;
}

} // namespace VisionInspector
