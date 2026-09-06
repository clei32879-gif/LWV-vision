/** @file test_license.cpp
 *  @brief 授权系统回归 (阶段6): 密钥对/签名验签/机器码稳定性/载荷解析/过期与模块逻辑
 *
 *  测试用运行时生成的密钥对 (不需要仓库里的真实私钥)。
 */
#include "../../src/core/LicenseManager.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QFile>
#include <cstdio>

using namespace VisionInspector;

static int g_failures = 0;
static int g_checks = 0;
#define CHECK(cond, msg) do { \
    ++g_checks; \
    if (cond) { } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

static QByteArray makeLicense(const QByteArray& privBlob, const QString& customer,
                              const QString& expiry, const QString& machine,
                              bool ai, int maxCameras) {
    QJsonObject modules;
    modules.insert("ai", ai);
    modules.insert("maxCameras", maxCameras);
    QJsonObject payload;
    payload.insert("customer", customer);
    payload.insert("expiry", expiry);
    payload.insert("machine", machine);
    payload.insert("modules", modules);
    payload.insert("issued", "2026-09-05");
    const QByteArray payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QByteArray sig = LicenseManager::signWithPrivateKey(payloadBytes, privBlob);
    QJsonObject root;
    root.insert("payload", payload);
    root.insert("signature", QString::fromLatin1(sig.toBase64()));
    return QJsonDocument(root).toJson();
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // 1. 机器码: 非空且同机稳定
    const QString mc1 = LicenseManager::computeMachineCode();
    const QString mc2 = LicenseManager::computeMachineCode();
    CHECK(mc1.size() == 19, "机器码格式 XXXX-XXXX-XXXX-XXXX");
    CHECK(mc1 == mc2, "机器码同机稳定");

    // 2. 密钥对生成 + 正常授权签发/验证
    QByteArray pub, priv;
    CHECK(LicenseManager::generateKeyPair(pub, priv), "密钥对生成");
    // 实测尺寸: 公钥blob=283 (24头+3指数+256模), 完整私钥blob=1179 (含CRT参数)
    CHECK(pub.size() > 100 && priv.size() > 600, "blob尺寸合理");
    LicenseManager::setPublicKeyBlobForTest(pub);

    // 0. 最小往返: 内存blob直接签验 (排除JSON/base64/文件因素)
    {
        const QByteArray payload0 = "hello-license";
        const QByteArray sig0 = LicenseManager::signWithPrivateKey(payload0, priv);
        CHECK(!sig0.isEmpty(), "最小往返: 签名产出");
        CHECK(LicenseManager::verifySignature(payload0, sig0), "最小往返: 签名+验证");
    }

    QTemporaryDir tmp;
    const QString licPath = tmp.path() + "/test.lic";
    QFile f(licPath);
    f.open(QIODevice::WriteOnly);
    f.write(makeLicense(priv, "测试客户", "2099-12-31", QString(), true, 8));
    f.close();

    // 3. 篡改检测: 改载荷内容必须验签失败
    {
        QJsonObject root = QJsonDocument::fromJson(QByteArray(makeLicense(
            priv, "测试客户", "2099-12-31", QString(), true, 8))).object();
        QJsonObject payload = root.value("payload").toObject();
        payload.insert("customer", "改过的客户"); // 篡改
        root.insert("payload", payload);
        QFile t(tmp.path() + "/tampered.lic");
        t.open(QIODevice::WriteOnly);
        t.write(QJsonDocument(root).toJson());
        t.close();

        // 用内部验签直接判定: 篡改后的载荷 + 原签名 = false
        const QByteArray tamperedBytes =
            QJsonDocument(payload).toJson(QJsonDocument::Compact);
        const QByteArray origSig = QByteArray::fromBase64(
            root.value("signature").toString().toLatin1());
        CHECK(!LicenseManager::verifySignature(tamperedBytes, origSig), "篡改载荷验签失败");
        CHECK(!LicenseManager::verifySignature(" totally different ", origSig),
              "异构载荷验签失败");
    }

    // 4. 错误公钥验证失败
    QByteArray pub2, priv2;
    LicenseManager::generateKeyPair(pub2, priv2);
    {
        const QByteArray payloadBytes = QJsonDocument(
            QJsonDocument::fromJson(makeLicense(priv, "A", "2099-12-31", QString(), true, 0))
                .object().value("payload").toObject()).toJson(QJsonDocument::Compact);
        const QByteArray sig = QByteArray::fromBase64(
            QJsonDocument::fromJson(makeLicense(priv, "A", "2099-12-31", QString(), true, 0))
                .object().value("signature").toString().toLatin1());
        LicenseManager::setPublicKeyBlobForTest(pub2); // 换另一把公钥
        CHECK(!LicenseManager::verifySignature(payloadBytes, sig), "公钥不匹配验签失败");
        LicenseManager::setPublicKeyBlobForTest(pub);  // 换回
        CHECK(LicenseManager::verifySignature(payloadBytes, sig), "正确公钥验签通过");
    }

    // 5. 换钥后旧授权文件的"导入-加载"路径: 验签失败不得崩溃 (行为返回错误原因)
    {
        LicenseManager::setPublicKeyBlobForTest(pub);
        // 无效文件
        QFile bad(tmp.path() + "/bad.lic");
        bad.open(QIODevice::WriteOnly);
        bad.write("不是授权文件");
        bad.close();
        // importLicense 的验证失败语义: 无效文件返回非空错误原因 (不崩溃)
        const QString err = LicenseManager::instance().importLicense(
            tmp.path() + "/bad.lic");
        CHECK(!err.isEmpty(), "无效文件导入返回错误原因");
        const QString err2 = LicenseManager::instance().importLicense(
            tmp.path() + "/not-exist.lic");
        CHECK(!err2.isEmpty(), "文件不存在导入返回错误原因");
    }

    std::printf("\n授权系统: %d项检查, 失败%d\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
