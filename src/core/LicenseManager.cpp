/** @file LicenseManager.cpp - 授权管理实现 (Windows CNG: bcrypt.dll) */
#include "LicenseManager.h"

#include "../utils/Logger.h"
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QSettings>
#include <QDateTime>

#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <cstdio>

#include "license_pubkey.h"   // kLicensePublicKeyBlobHex (公钥blob的hex, 编译期嵌入)

namespace VisionInspector {

// ============================================================
// CNG 基础设施
// ============================================================

static QByteArray cngHashSha256(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

static BCRYPT_ALG_HANDLE openAlg(LPCWSTR algId) {
    BCRYPT_ALG_HANDLE h = nullptr;
    if (FAILED(BCryptOpenAlgorithmProvider(&h, algId, nullptr, 0)))
        return nullptr;
    return h;
}

static QByteArray importAndSign(const QByteArray& payload, const QByteArray& privateBlob) {
    QByteArray empty;
    BCRYPT_ALG_HANDLE hRsa = openAlg(BCRYPT_RSA_SIGN_ALGORITHM);
    if (!hRsa) { std::fprintf(stderr, "[lic调试] openAlg失败\n"); return empty; }
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS st = BCryptImportKeyPair(hRsa, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, &hKey,
                                      reinterpret_cast<PUCHAR>(const_cast<char*>(privateBlob.constData())),
                                      ULONG(privateBlob.size()), 0);
    QByteArray sig;
    if (BCRYPT_SUCCESS(st)) {
        // 签名必须用完整私钥 (RSAPRIVATEBLOB 是无CRT半钥, SignHash会报INVALID_PARAMETER)
        BCRYPT_ALG_HANDLE hSha = openAlg(BCRYPT_SHA256_ALGORITHM);
        const QByteArray digest = cngHashSha256(payload);
        BCRYPT_PKCS1_PADDING_INFO pad;
        pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
        sig.resize(256); // RSA-2048 签名固定 256 字节
        ULONG cbDone = 0;
        st = BCryptSignHash(hKey, &pad,
                            reinterpret_cast<PUCHAR>(const_cast<char*>(digest.constData())),
                            ULONG(digest.size()),
                            reinterpret_cast<PUCHAR>(sig.data()), ULONG(sig.size()),
                            &cbDone, BCRYPT_PAD_PKCS1);
        if (!BCRYPT_SUCCESS(st)) sig.clear();
        else sig.resize(int(cbDone));
        if (hSha) BCryptCloseAlgorithmProvider(hSha, 0);
    }
    if (hKey) BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hRsa, 0);
    return sig;
}

static QByteArray g_publicKeyOverride; // 测试注入

static bool importAndVerify(const QByteArray& payload, const QByteArray& signature,
                            const QByteArray& publicBlob) {
    if (publicBlob.isEmpty() || signature.isEmpty()) return false;
    BCRYPT_ALG_HANDLE hRsa = openAlg(BCRYPT_RSA_SIGN_ALGORITHM); // 验签必须用 RSA_SIGN
    if (!hRsa) return false;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS st = BCryptImportKeyPair(hRsa, nullptr, BCRYPT_RSAPUBLIC_BLOB, &hKey,
                                      reinterpret_cast<PUCHAR>(const_cast<char*>(publicBlob.constData())),
                                      ULONG(publicBlob.size()), 0);
    bool ok = false;
    if (BCRYPT_SUCCESS(st)) {
        const QByteArray digest = cngHashSha256(payload);
        BCRYPT_PKCS1_PADDING_INFO pad;
        pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
        NTSTATUS vs = BCryptVerifySignature(
            hKey, &pad,
            reinterpret_cast<PUCHAR>(const_cast<char*>(digest.constData())),
            ULONG(digest.size()),
            reinterpret_cast<PUCHAR>(const_cast<char*>(signature.constData())),
            ULONG(signature.size()), BCRYPT_PAD_PKCS1);
        ok = BCRYPT_SUCCESS(vs);
        BCryptDestroyKey(hKey);
    } else {
        std::fprintf(stderr, "[lic调试] 公钥Import失败 st=0x%lX pub=%d\n",
                     (long)st, publicBlob.size());
    }
    BCryptCloseAlgorithmProvider(hRsa, 0);
    return ok;
}

static QByteArray hexToBytes(const char* hex) {
    QByteArray out;
    for (int i = 0; hex[i] && hex[i + 1]; i += 2) {
        bool ok = false;
        const int v = QString::fromLatin1(hex + i, 2).toInt(&ok, 16);
        if (!ok) return QByteArray();
        out.append(char(v));
    }
    return out;
}

// ============================================================
// 静态接口
// ============================================================

bool LicenseManager::verifySignature(const QByteArray& payload, const QByteArray& signature) {
    const QByteArray pub = g_publicKeyOverride.isEmpty()
        ? hexToBytes(kLicensePublicKeyBlobHex) : g_publicKeyOverride;
    return importAndVerify(payload, signature, pub);
}

QByteArray LicenseManager::signWithPrivateKey(const QByteArray& payload,
                                              const QByteArray& privateBlob) {
    return importAndSign(payload, privateBlob);
}

void LicenseManager::setPublicKeyBlobForTest(const QByteArray& blob) {
    g_publicKeyOverride = blob;
}

bool LicenseManager::generateKeyPair(QByteArray& publicBlob, QByteArray& privateBlob) {
    BCRYPT_ALG_HANDLE hRsa = openAlg(BCRYPT_RSA_SIGN_ALGORITHM);
    if (!hRsa) return false;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;
    do {
        if (!BCRYPT_SUCCESS(BCryptGenerateKeyPair(hRsa, &hKey, 2048, 0))) break;
        if (!BCRYPT_SUCCESS(BCryptFinalizeKeyPair(hKey, 0))) break;
        ULONG cb = 0;
        if (!BCRYPT_SUCCESS(BCryptExportKey(hKey, nullptr, BCRYPT_RSAPUBLIC_BLOB,
                                            nullptr, 0, &cb, 0))) break;
        publicBlob.resize(int(cb));
        if (!BCRYPT_SUCCESS(BCryptExportKey(hKey, nullptr, BCRYPT_RSAPUBLIC_BLOB,
                                            reinterpret_cast<PUCHAR>(publicBlob.data()),
                                            cb, &cb, 0))) break;
        if (!BCRYPT_SUCCESS(BCryptExportKey(hKey, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB,
                                            nullptr, 0, &cb, 0))) break;
        privateBlob.resize(int(cb));
        if (!BCRYPT_SUCCESS(BCryptExportKey(hKey, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB,
                                            reinterpret_cast<PUCHAR>(privateBlob.data()),
                                            cb, &cb, 0))) break;
        ok = true;
    } while (false);
    if (hKey) BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hRsa, 0);
    return ok;
}

// ============================================================
// 机器码: CPU品牌串 + 系统卷序列号 + 首网卡MAC → SHA256
// ============================================================

QString LicenseManager::computeMachineCode() {
    // CPU 品牌串 (CPUID 0x80000002~0x80000004)
    int regs[12] = {0};
    __cpuid(regs, 0x80000000);
    if (unsigned(regs[0]) >= 0x80000004) {
        __cpuid(regs + 0,  0x80000002);
        __cpuid(regs + 4,  0x80000003);
        __cpuid(regs + 8,  0x80000004);
    }
    const QString cpuBrand = QString::fromLatin1(reinterpret_cast<char*>(regs), 48)
                                 .remove(QChar(0)).trimmed();

    // 系统卷序列号
    DWORD volSerial = 0;
    GetVolumeInformationW(L"C:\\", nullptr, 0, &volSerial, nullptr, nullptr, nullptr, 0);

    // 首个非本回环网卡的 MAC
    QString mac;
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        if (!iface.hardwareAddress().isEmpty()) { mac = iface.hardwareAddress(); break; }
    }

    const QByteArray src = cpuBrand.toUtf8() + QByteArray::number(quint64(volSerial), 16)
                           + mac.toUtf8();
    const QByteArray digest = cngHashSha256(src);
    const QByteArray hex = digest.left(8).toHex().toUpper(); // 16位
    return QString::fromLatin1(hex.mid(0, 4) + "-" + hex.mid(4, 4) + "-"
                              + hex.mid(8, 4) + "-" + hex.mid(12, 4));
}

// ============================================================
// 状态与装载
// ============================================================

LicenseManager& LicenseManager::instance() {
    static LicenseManager inst;
    return inst;
}

void LicenseManager::initialize() {
    m_machineCode = computeMachineCode();
    ensureTrialStart();

    const QString path = QCoreApplication::applicationDirPath()
                         + QStringLiteral("/config/license.lic");
    if (QFileInfo::exists(path) && loadFromPath(path)) {
        VI_LOG_INFO(QString("授权已加载: %1, 到期 %2")
                        .arg(m_customer, m_expiry.date().toString(Qt::ISODate)));
    } else {
        // 试用期状态
        const qint64 days = m_firstRun.daysTo(QDateTime::currentDateTime());
        m_state = days > 30 ? State::Expired : State::Trial;
        VI_LOG_INFO(QString("无授权文件, 试用期状态: 已用%1天/%2")
                        .arg(days).arg(m_state == State::Expired ? "已过期" : "30天"));
    }
}

void LicenseManager::ensureTrialStart() {
    QSettings st(QStringLiteral("VisionInspector"), QStringLiteral("VisionInspector"));
    const QString key = QStringLiteral("firstRunAt");
    if (st.contains(key)) {
        m_firstRun = QDateTime::fromString(st.value(key).toString(), Qt::ISODate);
        if (!m_firstRun.isValid()) {
            m_firstRun = QDateTime::currentDateTime();
            st.setValue(key, m_firstRun.toString(Qt::ISODate));
        }
    } else {
        m_firstRun = QDateTime::currentDateTime();
        st.setValue(key, m_firstRun.toString(Qt::ISODate));
    }
}

bool LicenseManager::loadFromPath(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();
    const QByteArray signature = QByteArray::fromBase64(
        root.value(QStringLiteral("signature")).toString().toLatin1());
    if (payload.isEmpty() || signature.isEmpty()) return false;

    // 授权绑定机器 (machine 为空 = 不绑定, 便于试发)
    const QString machine = payload.value(QStringLiteral("machine")).toString();
    if (!machine.isEmpty() && machine != m_machineCode) {
        VI_LOG_WARN("授权文件与本机不匹配");
        return false;
    }

    // 载荷以紧凑JSON字节验签 (签发与验证同一序列化路径, Qt内键序稳定)
    const QByteArray payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (!verifySignature(payloadBytes, signature)) {
        VI_LOG_WARN("授权文件签名验证失败");
        return false;
    }

    const QDateTime expiry = QDateTime::fromString(
        payload.value(QStringLiteral("expiry")).toString(), Qt::ISODate);
    if (!expiry.isValid() || expiry < QDateTime::currentDateTime()) {
        VI_LOG_WARN("授权已过期");
        m_customer = payload.value(QStringLiteral("customer")).toString();
        m_expiry = expiry;
        m_state = State::Expired;
        return true; // 文件本身有效, 但已过期 → Expired 状态
    }

    m_customer = payload.value(QStringLiteral("customer")).toString();
    m_expiry = expiry;
    const QJsonObject modules = payload.value(QStringLiteral("modules")).toObject();
    m_modAi = modules.value(QStringLiteral("ai")).toBool(true);
    m_maxCameras = modules.value(QStringLiteral("maxCameras")).toInt(0);
    m_state = State::Licensed;
    return true;
}

LicenseManager::State LicenseManager::state() const { return m_state; }
QString LicenseManager::machineCode() const { return m_machineCode; }
QString LicenseManager::customer() const { return m_customer; }
QDateTime LicenseManager::expiry() const { return m_expiry; }

int LicenseManager::trialDaysLeft() const {
    const qint64 used = m_firstRun.daysTo(QDateTime::currentDateTime());
    return int(qMax<qint64>(0, 30 - used));
}

bool LicenseManager::moduleEnabled(const QString& key) const {
    if (m_state == State::Licensed && m_expiry < QDateTime::currentDateTime()) return false; // 过期停增值模块
    if (key == QStringLiteral("ai")) return m_modAi;
    return true; // 未知模块默认开
}

int LicenseManager::maxCameras() const {
    if (m_state != State::Licensed) return 0; // 试用不限
    return m_maxCameras;
}

QString LicenseManager::importLicense(const QString& filePath) {
    if (!QFileInfo::exists(filePath))
        return QStringLiteral("文件不存在");
    // 先用待导入文件验证 (不落盘)
    const QString target = QCoreApplication::applicationDirPath()
                           + QStringLiteral("/config/license.lic");
    QFile::remove(target);
    QFile::copy(filePath, target);
    if (!loadFromPath(target)) {
        QFile::remove(target);
        m_state = m_firstRun.daysTo(QDateTime::currentDateTime()) > 30
                      ? State::Expired : State::Trial;
        return QStringLiteral("授权文件无效: 签名验证失败或与本机不匹配");
    }
    VI_LOG_INFO("授权文件已导入: " + target);
    return {};
}

QString LicenseManager::statusText() const {
    switch (m_state) {
    case State::Licensed:
        return QStringLiteral("已授权: %1 (到期 %2)")
            .arg(m_customer, m_expiry.date().toString(QStringLiteral("yyyy-MM-dd")));
    case State::Trial:
        return QStringLiteral("试用期, 剩余 %1 天 (机器码 %2)")
            .arg(trialDaysLeft()).arg(m_machineCode);
    case State::Expired:
        return QStringLiteral("试用期已结束/授权过期 (机器码 %1) — 请导入授权文件")
            .arg(m_machineCode);
    }
    return {};
}

} // namespace VisionInspector
