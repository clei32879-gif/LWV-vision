/**
 * @file ToolRegistry.cpp
 * @brief 工具注册表实现
 */

#include "ToolRegistry.h"
#include <QDebug>

namespace VisionInspector {

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry instance;
    return instance;
}

bool ToolRegistry::registerTool(const QString& typeName, const ToolMetaData& meta) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_registry.contains(typeName)) {
        qWarning() << "工具类型已存在, 跳过注册:" << typeName;
        return false;
    }

    m_registry[typeName] = meta;
    return true;
}

bool ToolRegistry::unregisterTool(const QString& typeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.remove(typeName) > 0;
}

ITool* ToolRegistry::createTool(const QString& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_registry.find(typeName);
    if (it == m_registry.end()) {
        qWarning() << "工具类型未注册:" << typeName;
        return nullptr;
    }

    if (!it.value().createFunc) {
        qWarning() << "工具创建函数为空:" << typeName;
        return nullptr;
    }

    return it.value().createFunc();
}

ToolMetaData ToolRegistry::metaData(const QString& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.value(typeName);
}

QList<ToolMetaData> ToolRegistry::allMetaData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.values();
}

QList<ToolMetaData> ToolRegistry::metaDataByCategory(ToolCategory category) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    QList<ToolMetaData> result;
    for (const auto& meta : m_registry) {
        if (meta.category == category)
            result.append(meta);
    }
    return result;
}

bool ToolRegistry::isRegistered(const QString& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.contains(typeName);
}

int ToolRegistry::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.size();
}

void ToolRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry.clear();
}

} // namespace VisionInspector
