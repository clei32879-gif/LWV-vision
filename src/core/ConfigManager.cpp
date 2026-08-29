/**
 * @file ConfigManager.cpp
 * @brief 配置管理器实现 - JSON文件读写
 */

#include "ConfigManager.h"
#include "../utils/Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QCoreApplication>

namespace VisionInspector {

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
    , m_globalPath("./config/global.json")
{
    // 自动加载全局配置 (静默处理首次运行无文件的情况)
    loadGlobalConfig();
}

ConfigManager::~ConfigManager()
{
    saveGlobalConfig();
}

// ======== 配置路径 ========

void ConfigManager::setGlobalConfigPath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    m_globalPath = path;
}

void ConfigManager::setProjectConfigPath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    m_projectPath = path;
}

// ======== JSON 文件底层读写 ========

bool ConfigManager::readJsonFile(const QString& path, QJsonObject& json) const
{
    QFile file(path);
    if (!file.exists()) {
        return false; // 文件不存在不是错误
    }

    if (!file.open(QIODevice::ReadOnly)) {
        VI_LOG_WARN("无法打开配置文件(读): " + path);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.trimmed().isEmpty()) {
        json = QJsonObject();
        return true;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        VI_LOG_ERROR(QString("JSON解析错误 [%1]: %2 (offset=%3)")
            .arg(path, error.errorString()).arg(error.offset));
        return false;
    }

    if (!doc.isObject()) {
        VI_LOG_ERROR("配置根元素必须是对象: " + path);
        return false;
    }

    json = doc.object();
    return true;
}

bool ConfigManager::writeJsonFile(const QString& path, const QJsonObject& json) const
{
    if (!ensureConfigDir(path)) {
        VI_LOG_ERROR("无法创建配置目录: " + path);
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        VI_LOG_ERROR("无法写入配置文件: " + path);
        return false;
    }

    QJsonDocument doc(json);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool ConfigManager::ensureConfigDir(const QString& filePath) const
{
    QDir dir = QFileInfo(filePath).absoluteDir();
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

// ======== 加载配置 ========

bool ConfigManager::loadGlobalConfig()
{
    QMutexLocker locker(&m_mutex);

    QJsonObject json;
    if (readJsonFile(m_globalPath, json)) {
        m_globalConfig = json;
        VI_LOG_INFO("全局配置已加载: " + m_globalPath);
    } else {
        // 文件不存在, 初始化为空对象
        m_globalConfig = QJsonObject();
        VI_LOG_DEBUG("全局配置为空 (文件不存在): " + m_globalPath);
    }
    return true;
}

bool ConfigManager::loadProjectConfig(const QString& projectName)
{
    QMutexLocker locker(&m_mutex);

    if (projectName.isEmpty()) {
        m_projectConfig = QJsonObject();
        m_projectPath.clear();
        return true;
    }

    QString path = QString("./projects/%1/project.json").arg(projectName);
    m_projectPath = path;

    QJsonObject json;
    if (readJsonFile(path, json)) {
        m_projectConfig = json;
        VI_LOG_INFO("项目配置已加载: " + path);
    } else {
        m_projectConfig = QJsonObject();
        VI_LOG_DEBUG("项目配置为空: " + path);
    }
    return true;
}

// ======== 通用读写接口 ========

QVariant ConfigManager::get(const QString& key, const QVariant& defaultValue) const
{
    QMutexLocker locker(&m_mutex);

    // 项目配置优先
    if (!m_projectConfig.isEmpty()) {
        QVariant val = getNested(m_projectConfig, key);
        if (val.isValid() && !val.isNull()) {
            return val;
        }
    }

    // 回退全局配置
    QVariant val = getNested(m_globalConfig, key);
    if (val.isValid() && !val.isNull()) {
        return val;
    }

    return defaultValue;
}

void ConfigManager::set(const QString& key, const QVariant& value)
{
    {
        QMutexLocker locker(&m_mutex);
        m_globalConfig = setNestedValue(m_globalConfig, key, QJsonValue::fromVariant(value));
    }

    emit configChanged(key);
    saveGlobalConfig();
}

bool ConfigManager::has(const QString& key) const
{
    QMutexLocker locker(&m_mutex);

    QVariant gv = getNested(m_globalConfig, key);
    if (gv.isValid() && !gv.isNull()) return true;

    QVariant pv = getNested(m_projectConfig, key);
    return pv.isValid() && !pv.isNull();
}

void ConfigManager::remove(const QString& key)
{
    {
        QMutexLocker locker(&m_mutex);
        m_globalConfig = removeNestedValue(m_globalConfig, key);
    }
    emit configChanged(key);
}

QVariantMap ConfigManager::all() const
{
    QMutexLocker locker(&m_mutex);
    return m_globalConfig.toVariantMap();
}

QVariantMap ConfigManager::allProject() const
{
    QMutexLocker locker(&m_mutex);
    return m_projectConfig.toVariantMap();
}

// ======== 分组操作 ========

QVariantMap ConfigManager::group(const QString& groupName) const
{
    QMutexLocker locker(&m_mutex);

    QVariantMap merged;

    // 先合并全局分组
    auto git = m_globalConfig.constFind(groupName);
    if (git != m_globalConfig.constEnd() && git->isObject()) {
        QJsonObject go = git->toObject();
        for (auto it = go.constBegin(); it != go.constEnd(); ++it) {
            merged[it.key()] = it.value().toVariant();
        }
    }

    // 项目分组覆盖全局
    auto pit = m_projectConfig.constFind(groupName);
    if (pit != m_projectConfig.constEnd() && pit->isObject()) {
        QJsonObject po = pit->toObject();
        for (auto it = po.constBegin(); it != po.constEnd(); ++it) {
            merged[it.key()] = it.value().toVariant();
        }
    }

    return merged;
}

void ConfigManager::setGroup(const QString& groupName, const QVariantMap& values)
{
    {
        QMutexLocker locker(&m_mutex);

        QJsonObject groupObj;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            groupObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        m_globalConfig[groupName] = groupObj;
    }

    emit configChanged(groupName);
    saveGlobalConfig();
}

// ======== 持久化 ========

bool ConfigManager::saveGlobalConfig()
{
    QMutexLocker locker(&m_mutex);
    bool ok = writeJsonFile(m_globalPath, m_globalConfig);
    if (ok) {
        emit globalConfigSaved(m_globalPath);
    }
    return ok;
}

bool ConfigManager::saveProjectConfig()
{
    QMutexLocker locker(&m_mutex);
    if (m_projectPath.isEmpty()) {
        VI_LOG_WARN("项目配置路径为空, 跳过保存");
        return false;
    }
    bool ok = writeJsonFile(m_projectPath, m_projectConfig);
    if (ok) {
        emit projectConfigSaved(m_projectPath);
    }
    return ok;
}

void ConfigManager::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_globalConfig = QJsonObject();
    }
    VI_LOG_INFO("全局配置已清空");
    saveGlobalConfig();
}

void ConfigManager::clearProject()
{
    {
        QMutexLocker locker(&m_mutex);
        m_projectConfig = QJsonObject();
    }
    VI_LOG_INFO("项目配置已清空");
}

// ======== 嵌套 JSON 操作 (支持 "a.b.c" 点号路径) ========

QVariant ConfigManager::getNested(const QJsonObject& obj, const QString& key) const
{
    QStringList parts = key.split('.', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return QVariant();

    QJsonValue current(obj);
    for (const QString& part : parts) {
        if (!current.isObject()) return QVariant();
        QJsonObject curObj = current.toObject();
        auto it = curObj.constFind(part);
        if (it == curObj.constEnd()) return QVariant();
        current = *it;
    }

    return current.toVariant();
}

QJsonObject ConfigManager::setNestedValue(const QJsonObject& obj, const QString& key,
                                           const QJsonValue& value) const
{
    QStringList parts = key.split('.', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return obj;

    // 递归辅助: 构建从内到外的嵌套结构
    // 从叶子节点开始, 逐层向上包裹
    std::function<QJsonObject(int)> buildFrom;
    buildFrom = [&](int index) -> QJsonObject {
        if (index == parts.size() - 1) {
            // 叶子节点
            QJsonObject leaf;
            leaf[parts[index]] = value;
            return leaf;
        }

        QJsonObject inner = buildFrom(index + 1);
        QJsonObject current;
        current[parts[index]] = inner;
        return current;
    };

    // 从第0层开始合并
    return mergeJsonObjects(obj, buildFrom(0));
}

QJsonObject ConfigManager::mergeJsonObjects(const QJsonObject& base,
                                             const QJsonObject& overlay) const
{
    QJsonObject result = base;
    for (auto it = overlay.constBegin(); it != overlay.constEnd(); ++it) {
        const QString& key = it.key();
        const QJsonValue& overlayVal = it.value();

        if (result.contains(key) && result[key].isObject() && overlayVal.isObject()) {
            // 两者都是对象, 递归合并
            result[key] = mergeJsonObjects(result[key].toObject(), overlayVal.toObject());
        } else {
            // 覆盖或新增
            result[key] = overlayVal;
        }
    }
    return result;
}

QJsonObject ConfigManager::removeNestedValue(const QJsonObject& obj, const QString& key) const
{
    QStringList parts = key.split('.', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return obj;

    if (parts.size() == 1) {
        QJsonObject result = obj;
        result.remove(key);
        return result;
    }

    // 查找倒数第二层, 从那里删除叶子
    QJsonObject result = obj;
    QJsonObject* current = &result;

    // 遍历到倒数第二层
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString& part = parts[i];
        auto it = current->constFind(part);
        if (it == current->constEnd() || !it->isObject()) {
            return result; // 路径不存在
        }
        // 复制当前层的子对象
        QJsonObject child = it->toObject();
        (*current)[part] = child;
        // 重新获取对子对象的引用 (因为QJsonObject隐式共享)
        QJsonValueRef ref = (*current)[part];
        QJsonObject mutableChild = ref.toObject();
        ref = mutableChild;
    }

    // 现在在 current 的最后一层, 删除叶子
    // 重新构建...因为QJsonValueRef不提供直接修改嵌套对象的方法
    // 使用递归方式来实现
    std::function<QJsonObject(const QJsonObject&, int)> removeAt;
    removeAt = [&](const QJsonObject& node, int depth) -> QJsonObject {
        if (depth >= parts.size()) return node;

        const QString& part = parts[depth];
        QJsonObject result = node;

        if (depth == parts.size() - 1) {
            // 到达目标层级, 删除key
            result.remove(part);
            return result;
        }

        // 中间层: 递归处理子对象
        auto it = node.constFind(part);
        if (it != node.constEnd() && it->isObject()) {
            QJsonObject child = removeAt(it->toObject(), depth + 1);
            result[part] = child;
        }

        return result;
    };

    return removeAt(result, 0);
}

// ======== 快捷方法 ========

QString ConfigManager::defaultProjectDir() const
{
    return get("projectDir", QCoreApplication::applicationDirPath()).toString();
}

void ConfigManager::setDefaultProjectDir(const QString& dir)
{
    set("projectDir", dir);
}

bool ConfigManager::autoLoadLastProject() const
{
    return get("autoLoadLast", false).toBool();
}

void ConfigManager::setAutoLoadLastProject(bool enable)
{
    set("autoLoadLast", enable);
}

bool ConfigManager::showOnTop() const
{
    return get("showOnTop", false).toBool();
}

void ConfigManager::setShowOnTop(bool enable)
{
    set("showOnTop", enable);
}

QString ConfigManager::language() const
{
    return get("language", "zh_CN").toString();
}

void ConfigManager::setLanguage(const QString& lang)
{
    set("language", lang);
}

QString ConfigManager::lastProjectPath() const
{
    return get("lastProject", "").toString();
}

void ConfigManager::setLastProjectPath(const QString& path)
{
    set("lastProject", path);
}

int ConfigManager::windowWidth() const
{
    return get("window.width", 1280).toInt();
}

void ConfigManager::setWindowWidth(int width)
{
    set("window.width", width);
}

int ConfigManager::windowHeight() const
{
    return get("window.height", 800).toInt();
}

void ConfigManager::setWindowHeight(int height)
{
    set("window.height", height);
}

} // namespace VisionInspector
