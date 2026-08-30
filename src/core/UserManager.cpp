/** @file UserManager.cpp */
#include "UserManager.h"
#include "../utils/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QRandomGenerator>

namespace VisionInspector {

// ============================================================
// 密码安全存储 (H-7): 不再明文落盘
//   - 存储格式: "sha256$<salt>$<hexhash>" (盐+迭代哈希, 防彩虹表)
//   - 旧版明文自动识别并原地升级为哈希
//   - 空密码保持空(初始状态直接登录), 不落哈希
// ============================================================
namespace {

QString hashPassword(const QString& salt, const QString& password) {
    // 两次SHA-256 (加盐) 增强
    QByteArray data = salt.toUtf8() + ':' + password.toUtf8();
    QByteArray h = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    h = QCryptographicHash::hash(h + salt.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(h.toHex());
}

bool looksHashed(const QString& v) {
    return v.startsWith(QLatin1String("sha256$"));
}

QString newSalt() {
    const quint32 r = QRandomGenerator::global()->generate();
    return QStringLiteral("%1-%2").arg(r, 8, 16, QLatin1Char('0'))
                                  .arg(QRandomGenerator::global()->generate(), 8, 16, QLatin1Char('0'));
}

} // namespace

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
        const int idx = static_cast<int>(role);
        if (password.isEmpty()) {
            m_passwords[idx].clear();
        } else {
            const QString salt = newSalt();
            m_passwords[idx] = QStringLiteral("sha256$%1$%2").arg(salt, hashPassword(salt, password));
        }
        saveToDisk();
    }
}

bool UserManager::verifyPassword(UserRole role, const QString& password) const {
    const QString& stored = m_passwords[static_cast<int>(role)];
    // 如果密码为空, 允许直接登录(初始状态)
    if (stored.isEmpty()) return true;
    if (looksHashed(stored)) {
        // "sha256$salt$hash": 用存储的盐对输入重算比对
        const QStringList parts = stored.split('$');
        if (parts.size() != 3) return false;
        const QString salt = parts[1];
        const QString expect = parts[2];
        return hashPassword(salt, password) == expect;
    }
    // 兼容旧版明文 (load 时已升级, 此处仅防御)
    return stored == password;
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

    // H-7: 旧版明文 → 自动升级为哈希并回写
    bool upgraded = false;
    for (int i = 0; i < 3; ++i) {
        const QString& v = m_passwords[i];
        if (!v.isEmpty() && !looksHashed(v)) {
            const QString salt = newSalt();
            m_passwords[i] = QStringLiteral("sha256$%1$%2").arg(salt, hashPassword(salt, v));
            upgraded = true;
        }
    }
    if (upgraded) saveToDisk();
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
