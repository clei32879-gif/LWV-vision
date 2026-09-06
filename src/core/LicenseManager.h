/** @file LicenseManager.h - 授权管理 (阶段6商业交付: 机器码+签名授权文件+试用期)
 *
 *  授权模型 (对标 CKVision/DLCV 的桌面买断 + HALCON 式节点锁定):
 *    - 机器码: CPU品牌串 + 系统卷序列号 + 首个网卡MAC 的 SHA-256, 格式 XXXX-XXXX-XXXX-XXXX
 *    - 授权文件: JSON{customer, expiry, modules{ai, maxCameras}, machine, issued} + RSA2048签名
 *      验签公钥编译进程序 (license_pubkey.h), 私钥只存在供应商侧, 永不进仓库
 *    - 试用期: 无授权文件时, 首次运行起 30 天全功能; 过期不打断检测(现场断产是灾难),
 *      降级为 水印 + 状态栏提示 + AI模块停用
 *  密码学: Windows CNG (bcrypt.dll, 系统自带, 零第三方依赖)
 */
#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>

namespace VisionInspector {

class LicenseManager : public QObject {
    Q_OBJECT
public:
    enum class State { Licensed, Trial, Expired };

    static LicenseManager& instance();

    /** 启动时调用: 加载 config/license.lic (无则进入试用期状态) */
    void initialize();

    State state() const;
    QString machineCode() const;
    QString customer() const;
    QDateTime expiry() const;
    int trialDaysLeft() const;          // 试用期剩余天数 ( Licensed 时无意义 )
    QString statusText() const;         // 关于页/状态栏一句话

    /** 模块开关: "ai"=AI推理模块; 授权未包含或已过期 → false; 试用期全开 */
    bool moduleEnabled(const QString& key) const;
    /** 授权的相机路数上限 (0=不限); 试用=不限 */
    int maxCameras() const;

    /** 导入授权文件: 验证通过后拷贝到 config/license.lic 并生效;
     *  成功返回空串, 失败返回原因 (供对话框显示) */
    QString importLicense(const QString& filePath);

    // ── 底层 (license_gen 工具与单元测试复用) ──
    /** RSA2048-PKCS1-SHA256 验签 (使用内嵌公钥或 setPublicKeyBlob 注入的公钥) */
    static bool verifySignature(const QByteArray& payload, const QByteArray& signature);
    /** 用私钥blob签名 (license_gen 签发/测试注入私钥时用) */
    static QByteArray signWithPrivateKey(const QByteArray& payload,
                                         const QByteArray& privateBlob);
    /** 注入公钥 blob (单元测试用; 运行时勿调) */
    static void setPublicKeyBlobForTest(const QByteArray& blob);
    /** 非对称密钥对生成 (license_gen 用): 返回 {公钥blob, 私钥blob} */
    static bool generateKeyPair(QByteArray& publicBlob, QByteArray& privateBlob);
    /** 机器码 (稳定: 同一台机器每次相同) */
    static QString computeMachineCode();

private:
    LicenseManager() = default;
    bool loadFromPath(const QString& path);
    void ensureTrialStart();

    State m_state = State::Trial;
    QString m_customer;
    QDateTime m_expiry;
    bool m_modAi = true;          // 试用/默认: AI开
    int m_maxCameras = 0;         // 0=不限
    QString m_machineCode;
    QDateTime m_firstRun;
};

} // namespace VisionInspector
