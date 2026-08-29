#include "ProjectManager.h"
#include "../utils/Logger.h"
#include "../utils/Common.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>

namespace VisionInspector {

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent)
{
}

void ProjectManager::newProject() {
    m_currentPath.clear();
    m_projectName = QStringLiteral("New Project");
    m_projectNote.clear();
    m_modified = false;
    emit projectModified();
    VI_LOG_INFO("New project created");
}

bool ProjectManager::loadProject(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        VI_LOG_ERROR("Cannot open project file: " + path);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        VI_LOG_ERROR("Project file format error: " + error.errorString());
        return false;
    }

    QJsonObject json = doc.object();
    m_projectName = json.value("projectName").toString("Untitled");
    m_projectNote = json.value("projectNote").toString();
    m_currentPath = path;
    m_modified = false;

    // TODO: load flow, tools, UI config

    emit projectLoaded(path);
    VI_LOG_INFO("Project loaded: " + path);
    return true;
}

bool ProjectManager::saveProject(const QString& path) {
    QJsonObject json;
    json["projectName"] = m_projectName;
    json["projectNote"] = m_projectNote;
    json["version"] = versionString();

    // TODO: save flow, tools, UI config

    QJsonDocument doc(json);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        VI_LOG_ERROR("Cannot write project file: " + path);
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_currentPath = path;
    m_modified = false;

    emit projectSaved(path);
    VI_LOG_INFO("Project saved: " + path);
    return true;
}

} // namespace VisionInspector
