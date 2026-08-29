/**
 * @file PluginManager.cpp
 * @brief 插件管理器实现 - 动态加载/卸载插件
 */

#include "PluginManager.h"
#include "../engine/ITool.h"
#include "../engine/ToolRegistry.h"
#include "../hal/ICameraDriver.h"
#include "../hal/IPLCDriver.h"
#include "../utils/Logger.h"

#include <QPluginLoader>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QLibrary>

namespace VisionInspector {

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
    , m_pluginDir("./plugins")
{
}

PluginManager::~PluginManager()
{
    unloadAllPlugins();
}

void PluginManager::setPluginDirectory(const QString& dir)
{
    m_pluginDir = dir;
}

// ======== 扫描插件文件 ========

QStringList PluginManager::scanPluginFiles(const QDir& dir) const
{
    QStringList filters;
#ifdef Q_OS_WIN
    filters << "*.dll";
#else
    filters << "*.so";
#endif

    QStringList entries = dir.entryList(filters, QDir::Files | QDir::NoSymLinks);
    QStringList result;
    for (const QString& entry : entries) {
        result.append(dir.absoluteFilePath(entry));
    }

    // 也扫描子目录 (如 plugins/tools/, plugins/cameras/, plugins/logic/)
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subDir : subDirs) {
        QDir subDirPath(dir.absoluteFilePath(subDir));
        QStringList subEntries = subDirPath.entryList(filters, QDir::Files | QDir::NoSymLinks);
        for (const QString& entry : subEntries) {
            result.append(subDirPath.absoluteFilePath(entry));
        }
    }

    return result;
}

// ======== 加载插件 ========

int PluginManager::loadAllPlugins()
{
    int successCount = 0;
    int failCount = 0;

    QDir dir(m_pluginDir);
    if (!dir.exists()) {
        VI_LOG_WARN("插件目录不存在: " + m_pluginDir);
        // 尝试相对于可执行文件路径
        QString appDir = QCoreApplication::applicationDirPath();
        dir = QDir(appDir + "/plugins");
        if (!dir.exists()) {
            dir = QDir(appDir + "/../plugins");
        }
        if (!dir.exists()) {
            VI_LOG_WARN("找不到插件目录, 跳过加载");
            emit allPluginsLoaded(0, 0);
            return 0;
        }
        m_pluginDir = dir.absolutePath();
    }

    VI_LOG_INFO("从目录加载插件: " + dir.absolutePath());

    QStringList files = scanPluginFiles(dir);
    VI_LOG_INFO(QString("发现 %1 个插件文件").arg(files.size()));

    for (const QString& filePath : files) {
        if (loadPlugin(filePath)) {
            successCount++;
        } else {
            failCount++;
        }
    }

    VI_LOG_INFO(QString("插件加载完成: 成功 %1, 失败 %2").arg(successCount).arg(failCount));
    emit allPluginsLoaded(successCount, failCount);
    return successCount;
}

bool PluginManager::loadPlugin(const QString& filePath)
{
    // 避免重复加载
    if (m_loaders.contains(filePath)) {
        VI_LOG_DEBUG("插件已加载, 跳过: " + filePath);
        return true;
    }

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        VI_LOG_ERROR("插件文件不存在: " + filePath);
        return false;
    }

    PluginInfo info;
    info.fileName = fi.fileName();
    info.filePath = fi.absoluteFilePath();

    // 使用 QPluginLoader 加载
    auto* loader = new QPluginLoader(filePath, this);
    if (!loader->load()) {
        info.loaded = false;
        info.errorString = loader->errorString();
        m_plugins[filePath] = info;
        delete loader;
        VI_LOG_ERROR(QString("加载插件失败 [%1]: %2").arg(fi.fileName(), info.errorString));
        emit pluginLoaded(filePath, false);
        return false;
    }

    m_loaders[filePath] = loader;

    // 检查是否为工具插件 (实现了 ITool 接口)
    QObject* instance = loader->instance();
    if (!instance) {
        info.loaded = false;
        info.errorString = "无法创建插件实例: " + loader->errorString();
        m_plugins[filePath] = info;
        loader->unload();
        delete loader;
        m_loaders.remove(filePath);
        VI_LOG_ERROR(QString("创建插件实例失败 [%1]: %2").arg(fi.fileName(), info.errorString));
        emit pluginLoaded(filePath, false);
        return false;
    }

    // 尝试转换为各种接口
    ITool* toolIf = qobject_cast<ITool*>(instance);
    ICameraDriver* cameraIf = qobject_cast<ICameraDriver*>(instance);
    IPLCDriver* plcIf = qobject_cast<IPLCDriver*>(instance);

    info.loaded = true;

    // 记录工具类型 (通过 ToolRegistry 查找注册的工具)
    if (toolIf) {
        QString typeName = toolIf->typeName();
        if (!typeName.isEmpty()) {
            info.toolTypes.append(typeName);
        }
        // 注意: 插件的静态注册(VI_REGISTER_TOOL宏)在库加载时自动执行,
        // 这里只需要记录信息, 具体的工具创建通过 ToolRegistry 完成
        VI_LOG_DEBUG("已加载工具插件: " + typeName);
    }

    if (cameraIf) {
        info.cameraDriver = cameraIf->driverName();
        m_cameraDrivers[cameraIf->driverName()] = cameraIf;
        VI_LOG_DEBUG("已加载相机驱动: " + cameraIf->driverName());
    }

    if (plcIf) {
        info.plcDriver = plcIf->driverName();
        m_plcDrivers[plcIf->driverName()] = plcIf;
        VI_LOG_DEBUG("已加载PLC驱动: " + plcIf->driverName());
    }

    // 如果插件没有直接导出ITool实例, 检查ToolRegistry是否新增了工具类型
    // (通过 VI_REGISTER_TOOL 宏注册的工具, 注册发生在静态初始化阶段)
    {
        auto metas = ToolRegistry::instance().allMetaData();
        for (const auto& meta : metas) {
            if (!info.toolTypes.contains(meta.typeName)) {
                // 简单启发式: 如果文件名包含类型名, 则关联 (不完美但实用)
                QString lowerFile = fi.fileName().toLower();
                QString lowerType = meta.typeName.toLower();
                if (lowerFile.contains(lowerType)) {
                    info.toolTypes.append(meta.typeName);
                }
            }
        }
    }

    m_plugins[filePath] = info;

    VI_LOG_INFO(QString("插件加载成功: %1 (工具:%2 相机:%3 PLC:%4)")
        .arg(fi.fileName())
        .arg(info.toolTypes.join(","))
        .arg(info.cameraDriver.isEmpty() ? "-" : info.cameraDriver)
        .arg(info.plcDriver.isEmpty() ? "-" : info.plcDriver));

    emit pluginLoaded(filePath, true);
    return true;
}

// ======== 卸载插件 ========

void PluginManager::unloadAllPlugins()
{
    // 清理接口缓存
    m_cameraDrivers.clear();
    m_plcDrivers.clear();

    // 卸载所有插件加载器
    QStringList paths = m_loaders.keys();
    for (const QString& path : paths) {
        emit pluginAboutToUnload(path);
        QPluginLoader* loader = m_loaders.take(path);
        if (loader) {
            loader->unload();
            delete loader;
        }
    }

    m_plugins.clear();
    VI_LOG_INFO("所有插件已卸载");
}

bool PluginManager::unloadPlugin(const QString& filePath)
{
    if (!m_loaders.contains(filePath)) {
        return false;
    }

    emit pluginAboutToUnload(filePath);

    // 清理相关缓存
    PluginInfo info = m_plugins.value(filePath);
    if (!info.cameraDriver.isEmpty()) {
        m_cameraDrivers.remove(info.cameraDriver);
    }
    if (!info.plcDriver.isEmpty()) {
        m_plcDrivers.remove(info.plcDriver);
    }

    // 注销工具类型
    for (const QString& typeName : info.toolTypes) {
        ToolRegistry::instance().unregisterTool(typeName);
    }

    QPluginLoader* loader = m_loaders.take(filePath);
    if (loader) {
        loader->unload();
        delete loader;
    }

    m_plugins.remove(filePath);
    return true;
}

int PluginManager::reloadAllPlugins()
{
    unloadAllPlugins();
    return loadAllPlugins();
}

// ======== 查询接口 ========

PluginInfo PluginManager::pluginInfo(const QString& filePath) const
{
    return m_plugins.value(filePath);
}

// ======== 工具创建 ========

ITool* PluginManager::createTool(const QString& typeName)
{
    // 通过 ToolRegistry 创建 (插件通过 VI_REGISTER_TOOL 宏注册)
    ITool* tool = ToolRegistry::instance().createTool(typeName);
    if (tool) {
        VI_LOG_DEBUG("创建工具实例: " + typeName);
    } else {
        VI_LOG_WARN("无法创建工具实例: " + typeName + " (未注册或插件未加载)");
    }
    return tool;
}

bool PluginManager::hasTool(const QString& typeName) const
{
    return ToolRegistry::instance().isRegistered(typeName);
}

QStringList PluginManager::availableTools() const
{
    QStringList result;
    auto metas = ToolRegistry::instance().allMetaData();
    for (const auto& meta : metas) {
        result.append(meta.typeName);
    }
    return result;
}

// ======== 相机驱动 ========

ICameraDriver* PluginManager::getCameraDriver(const QString& driverName)
{
    // 先查缓存
    if (m_cameraDrivers.contains(driverName)) {
        return m_cameraDrivers[driverName];
    }

    // 遍历所有加载器查找相机驱动
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        QObject* instance = it.value()->instance();
        if (!instance) continue;

        ICameraDriver* cameraIf = qobject_cast<ICameraDriver*>(instance);
        if (cameraIf && cameraIf->driverName() == driverName) {
            m_cameraDrivers[driverName] = cameraIf;
            return cameraIf;
        }
    }

    VI_LOG_WARN("相机驱动未找到: " + driverName);
    return nullptr;
}

QStringList PluginManager::availableCameraDrivers() const
{
    return m_cameraDrivers.keys();
}

// ======== PLC驱动 ========

IPLCDriver* PluginManager::getPLCDriver(const QString& driverName)
{
    // 先查缓存
    if (m_plcDrivers.contains(driverName)) {
        return m_plcDrivers[driverName];
    }

    // 遍历所有加载器查找PLC驱动
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        QObject* instance = it.value()->instance();
        if (!instance) continue;

        IPLCDriver* plcIf = qobject_cast<IPLCDriver*>(instance);
        if (plcIf && plcIf->driverName() == driverName) {
            m_plcDrivers[driverName] = plcIf;
            return plcIf;
        }
    }

    VI_LOG_WARN("PLC驱动未找到: " + driverName);
    return nullptr;
}

QStringList PluginManager::availablePLCDrivers() const
{
    return m_plcDrivers.keys();
}

} // namespace VisionInspector
