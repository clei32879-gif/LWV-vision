/**
 * @file UserManager.h
 * @brief 用户权限管理
 * 三级权限: 操作员/技术员/管理员
 */
#pragma once
#include "../utils/Common.h"
#include <QObject>
#include <QString>

namespace VisionInspector {

class UserManager : public QObject {
    Q_OBJECT
public:
    explicit UserManager(QObject* parent = nullptr) : QObject(parent) {}

    UserRole currentRole() const { return m_role; }
    void setRole(UserRole role) { m_role = role; emit roleChanged(role); }

    bool login(UserRole targetRole, const QString& password);
    void logout() { m_role = UserRole::Operator; emit roleChanged(m_role); }

    void setPassword(UserRole role, const QString& password);
    bool verifyPassword(UserRole role, const QString& password) const;

    bool canEditFlow() const { return m_role == UserRole::Admin; }
    bool canEditUI() const { return m_role == UserRole::Admin; }
    bool canEditParams() const { return m_role >= UserRole::Technician; }

signals:
    void roleChanged(UserRole newRole);

private:
    UserRole m_role = UserRole::Admin;
    QString m_passwords[3]; // Operator, Technician, Admin
};

} // namespace VisionInspector
