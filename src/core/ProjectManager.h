/**
 * @file ProjectManager.h
 * @brief 项目管理器
 *
 * 管理检测项目的创建、加载、保存。
 * 每个项目文件(.vipj)包含: 流程配置、工具参数、界面布局、设置。
 */

#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

namespace VisionInspector {

class ProjectManager : public QObject {
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);

    /** 新建项目 */
    void newProject();

    /** 加载项目 */
    bool loadProject(const QString& path);

    /** 保存项目 */
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
};

} // namespace VisionInspector
