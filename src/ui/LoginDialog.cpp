/**
 * @file LoginDialog.cpp
 * @brief 用户切换登录对话框实现
 */

#include "LoginDialog.h"
#include "../utils/Common.h"
#include "../utils/Logger.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>

namespace VisionInspector {

LoginDialog::LoginDialog(UserManager* userManager, QWidget* parent)
    : QDialog(parent)
    , m_userMgr(userManager)
{
    setWindowTitle(QStringLiteral("切换用户"));
    auto* form = new QFormLayout(this);

    m_roleCombo = new QComboBox(this);
    m_roleCombo->addItem(roleToString(UserRole::Operator), int(UserRole::Operator));
    m_roleCombo->addItem(roleToString(UserRole::Technician), int(UserRole::Technician));
    m_roleCombo->addItem(roleToString(UserRole::Admin), int(UserRole::Admin));
    form->addRow(QStringLiteral("用户角色:"), m_roleCombo);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("未设置密码时可直接登录"));
    form->addRow(QStringLiteral("密码:"), m_passwordEdit);

    auto* changeBtn = new QPushButton(QStringLiteral("修改密码..."), this);
    form->addRow(QString(), changeBtn);
    connect(changeBtn, &QPushButton::clicked, this, &LoginDialog::onChangePassword);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &LoginDialog::onLogin);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void LoginDialog::onLogin() {
    const UserRole role = UserRole(m_roleCombo->currentData().toInt());
    if (m_userMgr->login(role, m_passwordEdit->text())) {
        VI_LOG_INFO(QString("用户登录: %1").arg(roleToString(role)));
        accept();
    } else {
        QMessageBox::warning(this, QStringLiteral("登录失败"),
                             QStringLiteral("密码错误, 请重试"));
    }
}

void LoginDialog::changePassword(QWidget* parent, UserManager* userManager) {
    // 先验证当前密码 (未设置密码的角色可直接修改)
    bool ok = false;
    const QString current = QInputDialog::getText(
        parent, QStringLiteral("验证身份"),
        QStringLiteral("输入当前密码 (未设置密码则留空):"),
        QLineEdit::Password, QString(), &ok);
    if (!ok) return;
    if (!userManager->verifyPassword(userManager->currentRole(), current)) {
        QMessageBox::warning(parent, QStringLiteral("错误"), QStringLiteral("当前密码不正确"));
        return;
    }

    QString newPassword;
    for (;;) {
        const QString p1 = QInputDialog::getText(
            parent, QStringLiteral("新密码"), QStringLiteral("输入新密码 (留空=不设密码):"),
            QLineEdit::Password, QString(), &ok);
        if (!ok) return;
        const QString p2 = QInputDialog::getText(
            parent, QStringLiteral("确认新密码"), QStringLiteral("再次输入新密码:"),
            QLineEdit::Password, QString(), &ok);
        if (!ok) return;
        if (p1 != p2) {
            QMessageBox::warning(parent, QStringLiteral("错误"), QStringLiteral("两次输入不一致"));
            continue;
        }
        newPassword = p1;
        break;
    }

    userManager->setPassword(userManager->currentRole(), newPassword);
    QMessageBox::information(parent, QStringLiteral("完成"), QStringLiteral("密码已修改并保存"));
    VI_LOG_INFO("用户密码已修改");
}

void LoginDialog::onChangePassword() {
    changePassword(this, m_userMgr);
}

} // namespace VisionInspector
