/**
 * @file UserManager.h
 * @brief 用户权限管理
 * 三级权限: 操作员/技术员/管理员
 * 密码持久化到 config/users.json (随工程目录携带)
 */
#pragma once
#include "../utils/Common.h"
#include <QObject>
#include <QString>

namespace VisionInspector {

class UserManager : public QObject {
    Q_OBJECT
public:
    explicit UserManager(QObject* parent = nullptr);

    UserRole currentRole() const { return m_role; }
    void setRole(UserRole role) { m_role = role; emit roleChanged(role); }

    bool login(UserRole targetRole, const QString& password);
    void logout() { m_role = UserRole::Operator; emit roleChanged(m_role); }

    void setPassword(UserRole role, const QString& password);
    bool verifyPassword(UserRole role, const QString& password) const;

    /** 是否已有自定义密码 (空密码=直接登录, 初始状态) */
    bool hasPassword(UserRole role) const;

    /** 从 config/users.json 加载 (启动时调用) */
    void loadFromDisk();
    /** 保存到 config/users.json */
    void saveToDisk();

signals:
    void roleChanged(UserRole newRole);

private:
    UserRole m_role = UserRole::Admin;
    QString m_passwords[3]; // Operator, Technician, Admin
};

} // namespace VisionInspector
