/** @file UserManager.cpp */
#include "UserManager.h"

namespace VisionInspector {

bool UserManager::login(UserRole targetRole, const QString& password) {
    if (verifyPassword(targetRole, password)) {
        m_role = targetRole;
        emit roleChanged(m_role);
        return true;
    }
    return false;
}

void UserManager::setPassword(UserRole role, const QString& password) {
    m_passwords[static_cast<int>(role)] = password;
}

bool UserManager::verifyPassword(UserRole role, const QString& password) const {
    const QString& stored = m_passwords[static_cast<int>(role)];
    // 如果密码为空, 允许直接登录(初始状态)
    return stored.isEmpty() || stored == password;
}

} // namespace VisionInspector
