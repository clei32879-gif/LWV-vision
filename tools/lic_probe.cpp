/** @file lic_probe.cpp — CNG 探针 (临时诊断用): 生成→签名→验证 全步骤状态码 */
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>

#define OK(st) ((((NTSTATUS)(st))) >= 0)

int main() {
    NTSTATUS st;
    BCRYPT_ALG_HANDLE hAlg = nullptr, hSha = nullptr;
    st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_SIGN_ALGORITHM, NULL, 0);
    printf("openAlg RSA_SIGN: 0x%lX\n", (long)st);
    st = BCryptOpenAlgorithmProvider(&hSha, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    printf("openAlg SHA256: 0x%lX\n", (long)st);

    BCRYPT_KEY_HANDLE hKey = nullptr;
    st = BCryptGenerateKeyPair(hAlg, &hKey, 2048, 0);
    printf("gen pair: 0x%lX\n", (long)st);
    st = BCryptFinalizeKeyPair(hKey, 0);
    printf("finalize: 0x%lX\n", (long)st);

    ULONG cb = 0;
    st = BCryptExportKey(hKey, NULL, BCRYPT_RSAPUBLIC_BLOB, NULL, 0, &cb, 0);
    printf("pub size: 0x%lX cb=%lu\n", (long)st, cb);
    UCHAR pub[1024]; ULONG cbPub = cb;
    st = BCryptExportKey(hKey, NULL, BCRYPT_RSAPUBLIC_BLOB, pub, cbPub, &cbPub, 0);
    printf("pub export: 0x%lX cb=%lu\n", (long)st, cbPub);

    st = BCryptExportKey(hKey, NULL, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, 0, &cb, 0);
    printf("priv size: 0x%lX cb=%lu\n", (long)st, cb);
    UCHAR priv[4096]; ULONG cbPriv = cb;
    st = BCryptExportKey(hKey, NULL, BCRYPT_RSAFULLPRIVATE_BLOB, priv, cbPriv, &cbPriv, 0);
    printf("priv export: 0x%lX cb=%lu\n", (long)st, cbPriv);

    // 哈希
    const char* msg = "hello-license";
    UCHAR digest[32]; ULONG cbHash = 0;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    st = BCryptCreateHash(hSha, &hHash, NULL, 0, NULL, 0, 0);
    printf("createHash: 0x%lX\n", (long)st);
    st = BCryptHashData(hHash, (PUCHAR)msg, (ULONG)strlen(msg), 0);
    printf("hashData: 0x%lX\n", (long)st);
    st = BCryptFinishHash(hHash, digest, 32, 0);
    printf("finishHash: 0x%lX\n", (long)st);

    // 签名 (用原始句柄上的私钥)
    UCHAR sig[256]; ULONG cbSig = 0;
    BCRYPT_PKCS1_PADDING_INFO pad; pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
    st = BCryptSignHash(hKey, &pad, digest, 32, sig, 256, &cbSig, BCRYPT_PAD_PKCS1);
    printf("sign(orig key): 0x%lX cbSig=%lu\n", (long)st, cbSig);

    // 导入公钥 → 验证
    BCRYPT_KEY_HANDLE hPub = nullptr;
    st = BCryptImportKeyPair(hAlg, NULL, BCRYPT_RSAPUBLIC_BLOB, &hPub, pub, cbPub, 0);
    printf("import pub: 0x%lX\n", (long)st);
    st = BCryptVerifySignature(hPub, &pad, digest, 32, sig, cbSig, BCRYPT_PAD_PKCS1);
    printf("verify(pub import): 0x%lX  %s\n", (long)st, OK(st) ? "== 验证通过" : "== 失败");

    // 也用原始私钥句柄验证
    st = BCryptVerifySignature(hKey, &pad, digest, 32, sig, cbSig, BCRYPT_PAD_PKCS1);
    printf("verify(orig key): 0x%lX\n", (long)st);

    // 导入私钥 → 再签一个 → 验证
    BCRYPT_KEY_HANDLE hPriv = nullptr;
    st = BCryptImportKeyPair(hAlg, NULL, BCRYPT_RSAFULLPRIVATE_BLOB, &hPriv, priv, cbPriv, 0);
    printf("import priv: 0x%lX\n", (long)st);
    UCHAR sig2[256]; ULONG cbSig2 = 0;
    st = BCryptSignHash(hPriv, &pad, digest, 32, sig2, 256, &cbSig2, BCRYPT_PAD_PKCS1);
    printf("sign(imported priv): 0x%lX cb=%lu\n", (long)st, cbSig2);
    st = BCryptVerifySignature(hPub, &pad, digest, 32, sig2, cbSig2, BCRYPT_PAD_PKCS1);
    printf("verify(imported pair): 0x%lX  %s\n", (long)st, OK(st) ? "== 验证通过" : "== 失败");
    return 0;
}
