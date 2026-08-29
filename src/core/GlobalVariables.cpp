/**
 * @file GlobalVariables.cpp
 * @brief 全局变量管理实现 - 线程安全
 */

#include "GlobalVariables.h"
#include "../utils/Logger.h"

namespace VisionInspector {

GlobalVariables::GlobalVariables(QObject* parent)
    : QObject(parent)
{
}

GlobalVariables::~GlobalVariables()
{
    clear();
}

// ======== 基本操作 (线程安全) ========

void GlobalVariables::set(const QString& name, const QVariant& value)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_vars.contains(name) || m_vars[name] != value) {
            m_vars[name] = value;
            changed = true;
        }
    }
    // 在锁外发射信号，避免死锁
    if (changed) {
        emit variableChanged(name, value);
    }
}

QVariant GlobalVariables::get(const QString& name, const QVariant& defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_vars.constFind(name);
    if (it != m_vars.constEnd()) {
        return it.value();
    }
    return defaultValue;
}

bool GlobalVariables::has(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_vars.contains(name);
}

void GlobalVariables::remove(const QString& name)
{
    bool existed = false;
    {
        QMutexLocker locker(&m_mutex);
        existed = m_vars.contains(name);
        m_vars.remove(name);
    }
    if (existed) {
        emit variableRemoved(name);
    }
}

void GlobalVariables::clear()
{
    bool hadItems = false;
    {
        QMutexLocker locker(&m_mutex);
        hadItems = !m_vars.isEmpty();
        m_vars.clear();
    }
    if (hadItems) {
        emit variablesCleared();
    }
}

QMap<QString, QVariant> GlobalVariables::all() const
{
    QMutexLocker locker(&m_mutex);
    return m_vars; // 返回副本
}

int GlobalVariables::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_vars.size();
}

QStringList GlobalVariables::keys() const
{
    QMutexLocker locker(&m_mutex);
    return m_vars.keys();
}

// ======== 便捷类型转换 ========

int GlobalVariables::getInt(const QString& name, int defaultValue) const
{
    return get(name, defaultValue).toInt();
}

double GlobalVariables::getDouble(const QString& name, double defaultValue) const
{
    return get(name, defaultValue).toDouble();
}

bool GlobalVariables::getBool(const QString& name, bool defaultValue) const
{
    return get(name, defaultValue).toBool();
}

QString GlobalVariables::getString(const QString& name, const QString& defaultValue) const
{
    return get(name, defaultValue).toString();
}

} // namespace VisionInspector
