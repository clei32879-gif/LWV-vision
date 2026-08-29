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
