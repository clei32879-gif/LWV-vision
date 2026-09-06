/**
 * @file ProjectManager.h
 * @brief 项目管理器
 *
 * 管理检测项目的创建、加载、保存。
 * 每个项目文件(.vipj, JSON)包含:
 *   - 项目信息 (名称/备注/版本)
 *   - 全部流程与工具 (含每个工具的属性、引用配置)
 *   - 全局变量
 *   - 节点布局位置
 */

#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

namespace VisionInspector {

class FlowEngine;
class GlobalVariables;

class ProjectManager : public QObject {
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);

    /** 注入服务 (MainWindow创建后调用, 保存/加载直接作用于真实数据) */
    void setServices(FlowEngine* engine, GlobalVariables* globals);

    /** 新建项目 (清空流程与全局变量) */
    void newProject();

    /** 加载项目 (重建全部流程与工具) */
    bool loadProject(const QString& path);

    /** 保存项目 (序列化全部流程与工具) */
    bool saveProject(const QString& path);

    /** 当前全部流程+全局变量快照 (撤销/重做用; 不含项目路径/名称) */
    QJsonObject currentStateJson() const;

    /** 从快照恢复流程与全局变量 (重建工具, 不改项目路径; 与加载走同一重建路径) */
    bool restoreStateJson(const QJsonObject& json);

    // ---- 设备模板 (阶段6: 模板=工程JSON, 存于 templates/ 目录, 用户可自建) ----

    /** 保存当前工程为模板文件 (不动当前项目路径, 与另存为的区别) */
    bool saveTemplate(const QString& path);

    /** 从模板新建: 重建全部流程/工具/全局变量, 但当前项目路径清空(未保存状态) */
    bool loadTemplate(const QString& path);

    /** 模板目录 (exe旁 templates/, 部署包随包携带) */
    static QString templateDir();

    /** 当前项目路径 */
    QString currentPath() const { return m_currentPath; }

    /** 项目名称 */
    QString projectName() const { return m_projectName; }
    void setProjectName(const QString& name) { m_projectName = name; }

    /** 项目备注 */
    QString projectNote() const { return m_projectNote; }
    void setProjectNote(const QString& note) { m_projectNote = note; }

    /** 是否已修改 (未保存) */
    bool isModified() const { return m_modified; }
    void markModified() { m_modified = true; }
    void clearModified() { m_modified = false; }

    /** 项目是否为空 */
    bool isEmpty() const { return m_currentPath.isEmpty() && m_projectName.isEmpty(); }

signals:
    void projectLoaded(const QString& path);
    void projectSaved(const QString& path);
    void projectModified();

private:
    QString m_currentPath;
    QString m_projectName;
    QString m_projectNote;
    bool m_modified = false;

    FlowEngine* m_engine = nullptr;
    GlobalVariables* m_globals = nullptr;
};

} // namespace VisionInspector
