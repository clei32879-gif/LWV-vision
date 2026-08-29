/**
 * @file ConfigManager.h
 * @brief 系统配置管理 - 基于JSON文件
 *
 * 全局配置: ./config/global.json
 * 项目配置: ./projects/<name>/project.json
 *
 * 支持 get/set/has/remove 操作，自动持久化。
 */

#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QJsonObject>
#include <QMutex>

namespace VisionInspector {

class ConfigManager : public QObject {
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() override;

    // ======== 配置路径 ========

    /** 设置全局配置文件路径 (默认 ./config/global.json) */
    void setGlobalConfigPath(const QString& path);

    /** 设置项目配置文件路径 */
    void setProjectConfigPath(const QString& path);

    /** 加载全局配置 */
    bool loadGlobalConfig();

    /** 加载项目配置 */
    bool loadProjectConfig(const QString& projectName);

    // ======== 通用读写 ========

    /** 获取配置值 (全局 → 项目, 项目优先) */
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /** 设置配置值 (写入全局配置) */
    void set(const QString& key, const QVariant& value);

    /** 检查key是否存在 */
    bool has(const QString& key) const;

    /** 删除配置项 */
    void remove(const QString& key);

    /** 获取所有全局配置 */
    QVariantMap all() const;

    /** 获取所有项目配置 */
    QVariantMap allProject() const;

    // ======== 分组操作 ========

    /** 获取某个分组的所有配置 */
    QVariantMap group(const QString& groupName) const;

    /** 设置分组配置 (合并方式) */
    void setGroup(const QString& groupName, const QVariantMap& values);

    // ======== 持久化 ========

    /** 保存全局配置到文件 */
    bool saveGlobalConfig();

    /** 保存项目配置到文件 */
    bool saveProjectConfig();

    /** 清空全局配置 */
    void clear();

    /** 清空项目配置 */
    void clearProject();

    // ======== 快捷方法 ========

    QString defaultProjectDir() const;
    void setDefaultProjectDir(const QString& dir);

    bool autoLoadLastProject() const;
    void setAutoLoadLastProject(bool enable);

    bool showOnTop() const;
    void setShowOnTop(bool enable);

    QString language() const;
    void setLanguage(const QString& lang);

    QString lastProjectPath() const;
    void setLastProjectPath(const QString& path);

    int windowWidth() const;
    void setWindowWidth(int width);

    int windowHeight() const;
    void setWindowHeight(int height);

signals:
    /** 配置项变化 */
    void configChanged(const QString& key);

    /** 全局配置已保存 */
    void globalConfigSaved(const QString& path);

    /** 项目配置已保存 */
    void projectConfigSaved(const QString& path);

private:
    /** 从JSON文件读取配置 */
    bool readJsonFile(const QString& path, QJsonObject& json) const;

    /** 写入JSON文件 */
    bool writeJsonFile(const QString& path, const QJsonObject& json) const;

    /** 根据点号路径获取嵌套值 (如 "window.size.width") */
    QVariant getNested(const QJsonObject& obj, const QString& key) const;

    /** 根据点号路径设置嵌套值 (返回新对象, 由于QJsonObject隐式共享) */
    QJsonObject setNestedValue(const QJsonObject& obj, const QString& key,
                                const QJsonValue& value) const;

    /** 根据点号路径删除嵌套值 (返回新对象) */
    QJsonObject removeNestedValue(const QJsonObject& obj, const QString& key) const;

    /** 深度合并两个JSON对象 */
    QJsonObject mergeJsonObjects(const QJsonObject& base,
                                  const QJsonObject& overlay) const;

    /** 确保配置目录存在 */
    bool ensureConfigDir(const QString& filePath) const;

    mutable QMutex m_mutex;

    QString m_globalPath;       // 全局配置路径
    QString m_projectPath;      // 项目配置路径
    QJsonObject m_globalConfig; // 全局配置数据
    QJsonObject m_projectConfig;// 项目配置数据
};

} // namespace VisionInspector
