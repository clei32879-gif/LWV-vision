/** @file UserManager.cpp */
#include "UserManager.h"
#include "../utils/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace VisionInspector {

UserManager::UserManager(QObject* parent)
    : QObject(parent)
{
    // 默认空密码: 初始状态各角色可直接登录
    loadFromDisk();
}

bool UserManager::login(UserRole targetRole, const QString& password) {
    if (verifyPassword(targetRole, password)) {
        m_role = targetRole;
        emit roleChanged(m_role);
        return true;
    }
    return false;
}

void UserManager::setPassword(UserRole role, const QString& password) {
    if (role == UserRole::Operator || role == UserRole::Technician
        || role == UserRole::Admin) {
        m_passwords[static_cast<int>(role)] = password;
        saveToDisk();
    }
}

bool UserManager::verifyPassword(UserRole role, const QString& password) const {
    const QString& stored = m_passwords[static_cast<int>(role)];
    // 如果密码为空, 允许直接登录(初始状态)
    return stored.isEmpty() || stored == password;
}

bool UserManager::hasPassword(UserRole role) const {
    return !m_passwords[static_cast<int>(role)].isEmpty();
}

void UserManager::loadFromDisk() {
    const QString path = QCoreApplication::applicationDirPath() + "/config/users.json";
    QFile file(path);
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) return;

    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    m_passwords[0] = json.value("operator").toString();
    m_passwords[1] = json.value("technician").toString();
    m_passwords[2] = json.value("admin").toString();
    VI_LOG_INFO("用户配置已加载: " + path);
}

void UserManager::saveToDisk() {
    const QString dir = QCoreApplication::applicationDirPath() + "/config";
    QDir().mkpath(dir);
    QFile file(dir + "/users.json");
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonObject json;
    json["operator"] = m_passwords[0];
    json["technician"] = m_passwords[1];
    json["admin"] = m_passwords[2];
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
}

} // namespace VisionInspector
