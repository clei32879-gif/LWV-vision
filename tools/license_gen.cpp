/** @file license_gen.cpp - 授权签发工具 (供应商侧专用, 不随产品打包)
 *
 *  用法:
 *    license_gen keygen [输出目录]        生成密钥对: license_pubkey.h(编译进产品)
 *                                         + license_private.key(绝密, 永不进仓库)
 *    license_gen show                     显示当前机器码
 *    license_gen sign <客户名> <YYYY-MM-DD> <机器码|-> <输出.lic> [ai=0|1] [maxCameras=N]
 *
 *  私钥文件仅保存在供应商电脑 (建议放在仓库目录之外), 泄露=授权体系作废。
 */
#include "../../src/core/LicenseManager.h"
#include "../../src/engine/ToolRegistry.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QByteArray>
#include <cstdio>

using namespace VisionInspector;

static int writePrivateKey(const QString& path, const QByteArray& blob) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return 1;
    f.write(blob.toBase64());
    return 0;
}

static QByteArray readPrivateKey(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QByteArray::fromBase64(f.readAll().trimmed());
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();

    if (args.size() >= 2 && args[1] == QStringLiteral("keygen")) {
        const QString dir = args.size() >= 3 ? args[2] : QStringLiteral(".");
        QByteArray pub, priv;
        if (!LicenseManager::generateKeyPair(pub, priv)) {
            std::printf("[错误] 密钥对生成失败\n");
            return 1;
        }
        // 公钥头文件 → 产品源码树 (公钥可入库)
        QString hex;
        for (unsigned char c : pub)
            hex += QString("%1").arg(int(c), 2, 16, QChar('0'));
        const QString header = QStringLiteral(
            "/** @file license_pubkey.h - 授权验签公钥 (license_gen keygen 生成) */\n"
            "#pragma once\n"
            "// RSA-2048 公钥 CNG blob, 私钥由供应商离线保管\n"
            "static const char kLicensePublicKeyBlobHex[] =\n    \"%1\";\n").arg(hex);
        const QString headerPath = dir + QStringLiteral("/license_pubkey.h");
        QFile hf(headerPath);
        if (!hf.open(QIODevice::WriteOnly)) { std::printf("[错误] 无法写入 %s\n", qPrintable(headerPath)); return 1; }
        hf.write(header.toUtf8());
        hf.close();

        // 私钥 → 仓库之外由用户自行保管
        const QString keyPath = dir + QStringLiteral("/license_private.key");
        writePrivateKey(keyPath, priv);
        std::printf("[完成] 公钥头: %s\n       私钥文件: %s (妥善保管, 不要提交进仓库!)\n",
                    qPrintable(headerPath), qPrintable(keyPath));
        return 0;
    }

    if (args.size() >= 2 && args[1] == QStringLiteral("show")) {
        std::printf("本机机器码: %s\n", qPrintable(LicenseManager::computeMachineCode()));
        return 0;
    }

    if (args.size() >= 6 && args[1] == QStringLiteral("sign")) {
        const QString customer = args[2];
        const QDateTime expiry = QDateTime::fromString(args[3], QStringLiteral("yyyy-MM-dd"));
        if (!expiry.isValid()) { std::printf("[错误] 到期日格式应为 YYYY-MM-DD\n"); return 1; }
        const QString machine = args[4] == QStringLiteral("-") ? QString() : args[4];

        QJsonObject modules;
        modules.insert(QStringLiteral("ai"), true);
        int maxCameras = 0;
        for (int i = 6; i < args.size(); ++i) {
            if (args[i].startsWith(QStringLiteral("ai=")))
                modules.insert(QStringLiteral("ai"), args[i].mid(3) == QStringLiteral("1"));
            else if (args[i].startsWith(QStringLiteral("maxCameras=")))
                maxCameras = args[i].mid(11).toInt();
        }
        modules.insert(QStringLiteral("maxCameras"), maxCameras);

        QJsonObject payload;
        payload.insert(QStringLiteral("customer"), customer);
        payload.insert(QStringLiteral("expiry"), expiry.date().toString(Qt::ISODate));
        payload.insert(QStringLiteral("machine"), machine);
        payload.insert(QStringLiteral("modules"), modules);
        payload.insert(QStringLiteral("issued"),
                       QDateTime::currentDateTime().date().toString(Qt::ISODate));

        const QByteArray payloadBytes =
            QJsonDocument(payload).toJson(QJsonDocument::Compact);

        // 私钥查找: 环境变量 LWVISION_KEY 或 ./license_private.key
        QString keyPath = qEnvironmentVariable("LWVISION_KEY");
        if (keyPath.isEmpty()) keyPath = QStringLiteral("license_private.key");
        const QByteArray priv = readPrivateKey(keyPath);
        if (priv.isEmpty()) { std::printf("[错误] 私钥未找到: %s\n", qPrintable(keyPath)); return 1; }

        const QByteArray sig = LicenseManager::signWithPrivateKey(payloadBytes, priv);
        if (sig.isEmpty()) { std::printf("[错误] 签名失败\n"); return 1; }

        QJsonObject root;
        root.insert(QStringLiteral("payload"), payload);
        root.insert(QStringLiteral("signature"), QString::fromLatin1(sig.toBase64()));
        const QString out = args[5];
        QFile f(out);
        if (!f.open(QIODevice::WriteOnly)) { std::printf("[错误] 无法写入 %s\n", qPrintable(out)); return 1; }
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
        std::printf("[完成] 授权已签发: %s\n  客户=%s 到期=%s 机器=%s\n",
                    qPrintable(out), qPrintable(customer),
                    qPrintable(args[3]), qPrintable(machine.isEmpty() ? "(不绑定)" : machine));
        return 0;
    }

    std::printf("LW Vision 授权签发工具\n"
                "  license_gen keygen [目录]                              生成密钥对\n"
                "  license_gen show                                       显示本机机器码\n"
                "  license_gen sign <客户> <YYYY-MM-DD> <机器码|-> <输出.lic> [ai=1] [maxCameras=8]\n");
    return 0;
}
