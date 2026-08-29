/**
 * @file PluginManager.h
 * @brief 插件管理器 - 动态加载/卸载插件
 *
 * 管理 plugins 目录下的所有 .so/.dll 插件。
 * 提供工具、相机驱动、PLC驱动的查询和创建接口。
 */

#pragma once
#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QDir>

class QPluginLoader;

namespace VisionInspector {

class ITool;
class ICameraDriver;
class IPLCDriver;

/** 已加载插件的描述信息 */
struct PluginInfo {
    QString fileName;       // 文件名 (如 "libBlobAnalysis.so")
    QString filePath;       // 完整路径
    bool loaded = false;    // 是否加载成功
    QString errorString;    // 加载失败原因
    QStringList toolTypes;  // 插件中注册的工具类型名
    QString cameraDriver;   // 插件中的相机驱动名(空=没有)
    QString plcDriver;      // 插件中的PLC驱动名(空=没有)
};

class PluginManager : public QObject {
    Q_OBJECT

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    // ======== 插件加载 ========

    /** 设置插件搜索目录 */
    void setPluginDirectory(const QString& dir);
    QString pluginDirectory() const { return m_pluginDir; }

    /** 扫描并加载目录下所有插件 */
    int loadAllPlugins();

    /** 加载单个插件 */
    bool loadPlugin(const QString& filePath);

    /** 卸载所有插件 */
    void unloadAllPlugins();

    /** 卸载单个插件 */
    bool unloadPlugin(const QString& filePath);

    /** 重新加载所有插件 */
    int reloadAllPlugins();

    // ======== 插件查询 ========

    /** 已加载的插件列表 */
    QList<PluginInfo> loadedPlugins() const { return m_plugins.values(); }

    /** 插件数量 */
    int pluginCount() const { return m_plugins.size(); }

    /** 查找插件信息 */
    PluginInfo pluginInfo(const QString& filePath) const;

    // ======== 工具查询 ========

    /** 创建指定类型的工具实例 (调用者拥有所有权) */
    ITool* createTool(const QString& typeName);

    /** 检查工具类型是否可用 */
    bool hasTool(const QString& typeName) const;

    /** 获取所有可用的工具类型名 */
    QStringList availableTools() const;

    // ======== 相机驱动查询 ========

    /** 获取相机驱动实例 (单例模式, 同一驱动只创建一次) */
    ICameraDriver* getCameraDriver(const QString& driverName);

    /** 获取所有可用相机驱动名 */
    QStringList availableCameraDrivers() const;

    // ======== PLC驱动查询 ========

    /** 获取PLC驱动实例 (单例模式) */
    IPLCDriver* getPLCDriver(const QString& driverName);

    /** 获取所有可用PLC驱动名 */
    QStringList availablePLCDrivers() const;

signals:
    /** 插件加载完成 */
    void pluginLoaded(const QString& filePath, bool success);

    /** 所有插件加载完成 */
    void allPluginsLoaded(int successCount, int failCount);

    /** 插件即将卸载 */
    void pluginAboutToUnload(const QString& filePath);

private:
    /** 扫描目录收集 .so/.dll 文件 */
    QStringList scanPluginFiles(const QDir& dir) const;

    /** 通过key查找已缓存的接口实例 */
    template<typename T>
    T* cachedInterface(const QMap<QString, T*>& cache, const QString& key) const;

    QString m_pluginDir;                          // 插件目录
    QMap<QString, PluginInfo> m_plugins;          // 路径 -> 插件信息
    QMap<QString, QPluginLoader*> m_loaders;      // 路径 -> 加载器

    // 接口实例缓存 (单例模式)
    QMap<QString, ICameraDriver*> m_cameraDrivers;
    QMap<QString, IPLCDriver*> m_plcDrivers;
};

} // namespace VisionInspector
