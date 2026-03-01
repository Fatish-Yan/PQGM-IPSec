# PQ-GM-IKEv2 问题修复记录

> **重要**: 每次修复问题后，必须在此文档记录详细信息。遇到问题时，先查阅此文档！

---

## 修复历史

### 2026-03-02: IKE_AUTH ECDSA 证书认证 + IntAuth 绑定验证

**问题**: 在证书认证模式下，SM2-KEM 私钥加载失败

**症状**:
```
SM2-KEM: failed to parse SM2 private key from DER
no trusted ECDSA public key found for 'initiator.pqgm.test'
```

**根因**:
1. SM2-KEM 尝试从 credential manager 获取私钥，但找到的是 ECDSA P-256 私钥（用于 IKE_AUTH）
2. ECDSA 私钥无法被解析为 SM2 格式
3. 证书缺少 SAN (Subject Alternative Name) 导致验证失败

**修复**:

1. **gmalg_ke.c**: 添加 ECDSA 格式解析失败时的文件 fallback
```c
/* Priority 3 解析失败时，尝试文件 fallback */
if (sm2_private_key_info_from_der(&sm2_my_key, ...) != 1)
{
    DBG1(DBG_IKE, "SM2-KEM: ECDSA key is not SM2 format, trying file fallback");
    goto try_file_fallback;
}
```

2. **证书生成**: 添加 SAN 扩展
```bash
# 添加 SAN 扩展
openssl x509 -req ... -extfile san.cnf -extensions v3_req
```

**验证结果**:
- ✅ IKE_AUTH ECDSA 证书认证成功
- ✅ IntAuth 绑定验证通过
- ✅ 5-RTT PQ-GM-IKEv2 完整流程验证通过

**详细记录**: [ike-auth-cert-verification-results.md](ike-auth-cert-verification-results.md)

---

### 2026-03-02: SM2 EncCert OID检查条件修复 (再次!)

**问题**: `ike_cert_post.c` 中检查 SM2 密钥的OID条件错误

**错误代码**:
```c
if (x509_key.algor == 17 ||  /* OID_sm2 */
    x509_key.algor == 19)    /* OID_ec_public_key */
```

**正确代码**:
```c
if (x509_key.algor == 18 &&  /* OID_ec_public_key */
    x509_key.algor_param == 5)    /* OID_sm2 */
```

**GmSSL 3.1.3 OID定义** (见 `/usr/local/include/gmssl/oid.h`):
```
OID_sm2 = 5           (曲线参数，作为 algor_param)
OID_ec_public_key = 18 (算法，作为 algor)
```

**X509_KEY结构** (见 `/usr/local/include/gmssl/x509_key.h`):
```c
typedef struct {
    int algor;        // 算法OID，如 OID_ec_public_key = 18
    int algor_param;  // 曲线参数，如 OID_sm2 = 5
    union {
        SM2_KEY sm2_key;
        // ...
    } u;
} X509_KEY;
```

**影响**:
- Responder无法识别Initiator的SM2 EncCert
- 导致SM2-KEM双向交换失败，Responder返回空响应

**修复文件**: `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_cert_post.c:881-882`

**注意**: 此问题在P0修复时应该已经解决，但由于上下文压缩导致记忆丢失，2026-03-02又重新发现并修复。

---

### 2026-03-01: P0 SM2公钥从EncCert提取

**问题**: SM2-KEM使用文件fallback而不是从EncCert提取公钥

**根因**: Base64解码未处理换行符 + OID检查值错误

**修复**:
1. `ike_cert_post.c`: 添加从IKE_INTERMEDIATE EncCert提取公钥的逻辑
2. `gmalg_ke.c`: 使用全局peer pubkey

**详细记录**: [p0-fix-record.md](p0-fix-record.md)

---

### 2026-03-01: P0.1 双向证书交换

**问题**: 只有Initiator发送证书，Responder不发送

**修复**: 修改Responder的`build_r`逻辑，在收到IKE_INTERMEDIATE时发送证书

---

### 2026-03-01: P2 IntAuth绑定验证

**验证**: strongSwan已实现RFC 9242 IntAuth机制

**结果**: IntAuth链正确工作，AUTH计算包含累积的IntAuth值

---

### 2026-03-01: P4 RFC 9370密钥更新链验证

**验证**: RFC 9370多重密钥交换后的密钥派生正确

**结果**: SKEYSEED链式更新正确，SK_*每轮重新派生

---

## 常见错误速查表

| 错误现象 | 可能原因 | 解决方案 |
|---------|---------|---------|
| `EncCert key is not SM2 (algor=18)` | OID检查条件错误 | 检查 `algor == 18 && algor_param == 5` |
| `OpenSSL X.509 parsing failed` | strongSwan不支持SM2证书 | 正常，代码直接从文件读取 |
| `SM2-KEM: no peer pubkey available` | EncCert提取失败 | 检查证书KeyUsage和OID |
| Responder返回空响应 | 无法识别对端EncCert | 检查OID检查条件 |

---

### 2026-03-02: Task 11 ML-DSA证书生成

**问题**: 系统OpenSSL 3.0.2不支持ML-DSA证书生成

**当前状态**:
- OpenSSL 3.0.2 (需要3.5+才能支持ML-DSA)
- liboqs 0.12.0已安装

**解决方案**:
1. 创建`scripts/generate_mldsa_certs.sh` - 用于OpenSSL 3.5+环境
2. 创建`scripts/generate_mldsa_raw_keys.c` - 使用liboqs生成原始密钥对(当前系统可用)

**生成的ML-DSA-65密钥**:
- Initiator: `/home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa/`
- Responder: `/home/ipsec/PQGM-IPSec/docker/responder/certs/mldsa/`
- 公钥大小: 1952字节
- 私钥大小: 4032字节
- 签名大小: 3309字节

**已知限制**:
- 当前系统无法生成X.509格式的ML-DSA证书
- 需要升级到OpenSSL 3.5+并安装oqs-provider才能生成证书
- 原始密钥对可用于ML-DSA签名功能测试

**详细记录**: [MLDSA_CERTS_README.md](../scripts/MLDSA_CERTS_README.md)

---

## 关键代码位置

| 功能 | 文件 |
|------|------|
| EncCert提取和SM2公钥设置 | `ike_cert_post.c` |
| SM2-KEM加解密 | `gmalg_ke.c` |
| IKE_INTERMEDIATE处理 | `ike_init.c` |
| IntAuth计算 | `keymat_v2.c` |
| RFC 9370密钥派生 | `keymat_v2.c` |
| ML-DSA签名器 | `mldsa_signer.c/h` |
| ML-DSA证书生成 | `scripts/generate_mldsa_*.c` |
