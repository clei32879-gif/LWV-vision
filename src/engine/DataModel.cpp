/**
 * @file DataModel.cpp
 * @brief 数据流模型实现 - 工具间数据传递
 */

#include "DataModel.h"
#include "ToolContext.h"
#include "../utils/Logger.h"

#include <QMutexLocker>
#include <QMetaType>

namespace VisionInspector {

DataModel::DataModel(QObject* parent)
    : QObject(parent)
{
}

DataModel::~DataModel()
{
    clear();
}

// ======== 内部辅助 ========

QString DataModel::makeKey(const QString& toolName, const QString& dataKey)
{
    return toolName + "." + dataKey;
}

bool DataModel::isValidType(const QVariant& value) const
{
    if (!value.isValid()) return false;

    switch (static_cast<QMetaType::Type>(value.typeId())) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Bool:
    case QMetaType::QString:
    case QMetaType::QImage:
    case QMetaType::QPoint:
    case QMetaType::QPointF:
    case QMetaType::QRect:
    case QMetaType::QRectF:
    case QMetaType::QPolygon:
    case QMetaType::QPolygonF:
        return true;
    default:
        return false;
    }
}

// ======== 数据写入 ========

void DataModel::setData(const QString& toolName, const QString& dataKey, const QVariant& value)
{
    if (toolName.isEmpty() || dataKey.isEmpty()) {
        VI_LOG_WARN("DataModel::setData: 工具名或数据键为空");
        return;
    }

    DataEntry entry;
    entry.toolName = toolName;
    entry.dataKey = dataKey;
    entry.value = value;
    entry.typeId = value.typeId();

    QString fullKey;
    {
        QMutexLocker locker(&m_mutex);
        fullKey = makeKey(toolName, dataKey);
        m_entries[fullKey] = entry;
    }

    emit dataSet(toolName, dataKey, value);
}

void DataModel::setInt(const QString& toolName, const QString& key, int value)
{
    setData(toolName, key, QVariant(value));
}

void DataModel::setDouble(const QString& toolName, const QString& key, double value)
{
    setData(toolName, key, QVariant(value));
}

void DataModel::setString(const QString& toolName, const QString& key, const QString& value)
{
    setData(toolName, key, QVariant(value));
}

void DataModel::setBool(const QString& toolName, const QString& key, bool value)
{
    setData(toolName, key, QVariant(value));
}

void DataModel::setImage(const QString& toolName, const QString& key, const QImage& value)
{
    setData(toolName, key, QVariant(value));
}

void DataModel::setPoint(const QString& toolName, const QString& key, const QPoint& value)
{
    setData(toolName, key, QVariant(value));
}

void DataModel::setRect(const QString& toolName, const QString& key, const QRect& value)
{
    setData(toolName, key, QVariant(value));
}

void DataModel::setPolygon(const QString& toolName, const QString& key, const QPolygon& value)
{
    setData(toolName, key, QVariant(value));
}

// ======== 数据读取 ========

QVariant DataModel::getData(const QString& fullKey, const QVariant& defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_entries.constFind(fullKey);
    if (it != m_entries.constEnd()) {
        return it.value().value;
    }
    return defaultValue;
}

QVariant DataModel::getData(const QString& toolName, const QString& dataKey,
                             const QVariant& defaultValue) const
{
    return getData(makeKey(toolName, dataKey), defaultValue);
}

int DataModel::getInt(const QString& toolName, const QString& key, int defaultValue) const
{
    QVariant v = getData(toolName, key);
    if (!v.isValid()) return defaultValue;
    bool ok = false;
    int result = v.toInt(&ok);
    return ok ? result : defaultValue;
}

double DataModel::getDouble(const QString& toolName, const QString& key, double defaultValue) const
{
    QVariant v = getData(toolName, key);
    if (!v.isValid()) return defaultValue;
    bool ok = false;
    double result = v.toDouble(&ok);
    return ok ? result : defaultValue;
}

QString DataModel::getString(const QString& toolName, const QString& key,
                              const QString& defaultValue) const
{
    QVariant v = getData(toolName, key);
    if (!v.isValid()) return defaultValue;
    return v.toString();
}

bool DataModel::getBool(const QString& toolName, const QString& key, bool defaultValue) const
{
    QVariant v = getData(toolName, key);
    if (!v.isValid()) return defaultValue;
    return v.toBool();
}

QImage DataModel::getImage(const QString& toolName, const QString& key) const
{
    QVariant v = getData(toolName, key);
    if (v.isValid() && v.canConvert<QImage>()) {
        return v.value<QImage>();
    }
    return QImage();
}

QPoint DataModel::getPoint(const QString& toolName, const QString& key) const
{
    QVariant v = getData(toolName, key);
    if (v.isValid() && v.canConvert<QPoint>()) {
        return v.toPoint();
    }
    return QPoint();
}

QRect DataModel::getRect(const QString& toolName, const QString& key) const
{
    QVariant v = getData(toolName, key);
    if (v.isValid() && v.canConvert<QRect>()) {
        return v.toRect();
    }
    return QRect();
}

QPolygon DataModel::getPolygon(const QString& toolName, const QString& key) const
{
    QVariant v = getData(toolName, key);
    if (v.isValid() && v.canConvert<QPolygon>()) {
        return v.value<QPolygon>();
    }
    return QPolygon();
}

// ======== 数据查询 ========

bool DataModel::hasData(const QString& toolName, const QString& dataKey) const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.contains(makeKey(toolName, dataKey));
}

QMap<QString, QVariant> DataModel::toolOutputs(const QString& toolName) const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, QVariant> result;
    QString prefix = toolName + ".";

    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (it.key().startsWith(prefix)) {
            result[it.value().dataKey] = it.value().value;
        }
    }
    return result;
}

QList<DataEntry> DataModel::allEntries() const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.values();
}

int DataModel::entryCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.size();
}

QStringList DataModel::toolNames() const
{
    QMutexLocker locker(&m_mutex);
    QSet<QString> names;
    for (const auto& entry : m_entries) {
        names.insert(entry.toolName);
    }
    return names.values();
}

QStringList DataModel::dataKeys(const QString& toolName) const
{
    QMutexLocker locker(&m_mutex);
    QStringList keys;
    QString prefix = toolName + ".";
    for (const auto& entry : m_entries) {
        if (entry.toolName == toolName) {
            keys.append(entry.dataKey);
        }
    }
    return keys;
}

// ======== 与 ToolContext 交互 ========

void DataModel::importFromContext(const ToolContext& context, const QString& toolName)
{
    const DataMap& data = context.allData();
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        setData(toolName, it.key(), it.value());
    }
}

void DataModel::exportToContext(ToolContext& context) const
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const DataEntry& entry = it.value();
        // 导出时也使用完整键, 方便在上下文中区分来源
        context.setData(entry.fullKey(), entry.value);
    }
}

// ======== 清理 ========

void DataModel::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_entries.clear();
    }
    emit allDataCleared();
    VI_LOG_DEBUG("DataModel: 所有数据已清空");
}

void DataModel::clearTool(const QString& toolName)
{
    {
        QMutexLocker locker(&m_mutex);
        QString prefix = toolName + ".";
        QList<QString> keysToRemove;
        for (const auto& entry : m_entries) {
            if (entry.toolName == toolName) {
                keysToRemove.append(makeKey(entry.toolName, entry.dataKey));
            }
        }
        for (const QString& key : keysToRemove) {
            m_entries.remove(key);
        }
    }
    emit dataCleared(toolName);
}

// ======== 序列化 ========

QVariantMap DataModel::toVariantMap() const
{
    QMutexLocker locker(&m_mutex);
    QVariantMap result;

    // 按工具名分组
    QMap<QString, QVariantMap> grouped;
    for (const auto& entry : m_entries) {
        grouped[entry.toolName][entry.dataKey] = entry.value;
    }

    // 转换为扁平 QVariantMap (用嵌套QVariantMap表示分组)
    for (auto git = grouped.constBegin(); git != grouped.constEnd(); ++git) {
        result[git.key()] = git.value();
    }

    return result;
}

void DataModel::fromVariantMap(const QVariantMap& map)
{
    QMutexLocker locker(&m_mutex);
    m_entries.clear();

    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        const QString& toolName = it.key();
        QVariantMap toolData = it.value().toMap();

        for (auto dit = toolData.constBegin(); dit != toolData.constEnd(); ++dit) {
            DataEntry entry;
            entry.toolName = toolName;
            entry.dataKey = dit.key();
            entry.value = dit.value();
            entry.typeId = dit.value().typeId();
            m_entries[makeKey(toolName, dit.key())] = entry;
        }
    }
}

} // namespace VisionInspector
