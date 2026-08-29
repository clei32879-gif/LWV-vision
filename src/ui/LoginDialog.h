/**
 * @file LoginDialog.h
 * @brief 用户切换登录对话框 — 三级权限(操作员/技术员/管理员)
 */
#pragma once

#include "../core/UserManager.h"
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>

namespace VisionInspector {

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(UserManager* userManager, QWidget* parent = nullptr);
    /** 提供修改当前角色密码的入口 */
    static void changePassword(QWidget* parent, UserManager* userManager);

private slots:
    void onLogin();
    void onChangePassword();

private:
    UserManager* m_userMgr = nullptr;
    QComboBox* m_roleCombo = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
};

} // namespace VisionInspector
