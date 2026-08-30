/**
 * @file test_usermanager.cpp
 * @brief 用户管理密码安全存储回归 (H-7)
 *
 * 验证:
 *   1. setPassword 后落盘为哈希 (sha256$salt$hash), 不含明文
 *   2. 正确密码可登录, 错误密码拒绝
 *   3. 空密码(初始状态) 直接登录
 *   4. 旧版明文 users.json 加载时自动升级为哈希并回写
 *   5. 持久化: 重新构造 UserManager 仍能验证
 */
#include "../../src/core/UserManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;
static int g_checks = 0;
#define CHECK(cond, msg) do { \
    ++g_checks; \
    if (cond) { } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

static QString usersPath() {
    return QCoreApplication::applicationDirPath() + "/config/users.json";
}

static void cleanupUsersFile() {
    const QString p = usersPath();
    if (QFile::exists(p)) QFile::remove(p);
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    cleanupUsersFile();

    // ---- 1. 哈希存储 ----
    {
        UserManager um;
        um.setPassword(UserRole::Admin, "s3cr3tP@ss");
        CHECK(um.hasPassword(UserRole::Admin), "哈希: admin已有密码");
        CHECK(um.verifyPassword(UserRole::Admin, "s3cr3tP@ss"), "哈希: 正确密码验证通过");
        CHECK(!um.verifyPassword(UserRole::Admin, "wrong"), "哈希: 错误密码拒绝");
        CHECK(um.login(UserRole::Admin, "s3cr3tP@ss"), "哈希: 正确密码登录成功");
        CHECK(!um.login(UserRole::Admin, "bad"), "哈希: 错误密码登录失败");
    }

    // ---- 2. 落盘内容为哈希且不含明文 ----
    {
        const QString p = usersPath();
        CHECK(QFile::exists(p), "哈希: users.json已写入");
        QFile f(p);
        if (f.open(QIODevice::ReadOnly)) {
            const QString content = QString::fromUtf8(f.readAll());
            CHECK(content.contains("sha256$"), "哈希: 存储格式含sha256$盐前缀");
            CHECK(!content.contains("s3cr3tP@ss"), "哈希: 明文未落盘");
            CHECK(content.contains("admin"), "哈希: admin字段存在");
        }
    }

    // ---- 3. 持久化: 重新构造仍可验证 ----
    {
        UserManager um2;
        CHECK(um2.verifyPassword(UserRole::Admin, "s3cr3tP@ss"), "持久化: 重启后密码仍有效");
        CHECK(!um2.verifyPassword(UserRole::Admin, "wrong"), "持久化: 重启后错误密码仍拒绝");
    }

    // ---- 4. 旧版明文自动升级 ----
    {
        const QString dir = QCoreApplication::applicationDirPath() + "/config";
        QDir().mkpath(dir);
        {
            QFile f(usersPath());
            if (f.open(QIODevice::WriteOnly)) {
                f.write("{\n  \"operator\": \"\",\n  \"technician\": \"1234\",\n  \"admin\": \"\"\n}\n");
            }
        } // 关闭写句柄, 否则 Windows 下后续只读打开失败
        UserManager um3;
        CHECK(um3.verifyPassword(UserRole::Technician, "1234"), "升级: 旧明文密码仍可验证");
        CHECK(!um3.verifyPassword(UserRole::Technician, "9999"), "升级: 错误密码拒绝");
        // 回写后应为哈希
        QFile f2(usersPath());
        if (f2.open(QIODevice::ReadOnly)) {
            const QString content = QString::fromUtf8(f2.readAll());
            CHECK(content.contains("sha256$"), "升级: 回写后为哈希");
            CHECK(!content.contains("\"1234\""), "升级: 回写后不含明文");
        }
    }

    // ---- 5. 空密码初始状态 ----
    {
        cleanupUsersFile();
        UserManager um4;
        CHECK(!um4.hasPassword(UserRole::Operator), "空密码: 初始无密码");
        CHECK(um4.verifyPassword(UserRole::Operator, ""), "空密码: 空输入直接登录");
    }

    cleanupUsersFile();
    std::printf("用户管理回归: %d项检查, 失败%d\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
