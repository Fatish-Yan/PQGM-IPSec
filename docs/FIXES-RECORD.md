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

### 2026-03-02: ML-DSA-65 Signer Plugin Implementation

**功能**: 为 strongSwan 添加 ML-DSA-65 后量子签名支持

**实现**:
- 创建独立 mldsa 插件，参考 gmalg 模式
- 使用 liboqs 进行 ML-DSA-65 签名/验证
- 算法 ID: AUTH_MLDSA_65 = 1053 (私有使用范围)

**文件**:
- strongswan/src/libstrongswan/plugins/mldsa/
  - mldsa_plugin.c/h: 插件注册
  - mldsa_signer.c/h: ML-DSA-65 签名器实现

**验证结果**:
- ✅ liboqs 0.12.0 安装完成
- ✅ 插件编译成功
- ✅ 插件加载成功
- ✅ 单元测试通过 (ML-DSA-65 sign/verify)
- ⏳ IKE_AUTH 集成测试 (需要 OpenSSL 3.5+ 支持 ML-DSA 证书)

**已知限制**:
- OpenSSL 3.0.2 不支持 ML-DSA 证书生成
- 需要 OpenSSL 3.5+ 或 oqs-provider 才能进行完整测试

**详细记录**: [MLDSA-IKE-AUTH-TEST-STATUS.md](MLDSA-IKE-AUTH-TEST-STATUS.md)

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

---

### 2026-03-02: ML-DSA Private Key Loader 修复

**问题**: `mldsa_private_key.c` 编译错误，无法正确实现 `private_key_t` 接口

**错误列表**:
1. `private_key_t` 没有 `get_refcount` 成员 - 应该使用 `get_ref`
2. 缺少 `get_keysize` 方法
3. 缺少 `get_public_key` 方法
4. 缺少 `has_fingerprint` 方法
5. `chunk_read_file` 隐式声明 - 应该使用 `chunk_map/chunk_unmap`
6. `hasher->allocate_hash` 返回值未使用

**修复内容**:

1. **添加正确的 private_key_t 方法实现**:
```c
METHOD(private_key_t, get_keysize, int, ...)
METHOD(private_key_t, get_public_key, public_key_t*, ...)
METHOD(private_key_t, has_fingerprint, bool, ...)
METHOD(private_key_t, get_ref, private_key_t*, ...)
// 移除 get_refcount
```

2. **使用 chunk_map 替代 chunk_read_file**:
```c
// 替代: chunk_read_file(file, &data)
mapped = chunk_map(file, FALSE);
if (mapped)
{
    key = mldsa_private_key_create(*mapped);
    chunk_unmap(mapped);
}
```

3. **修复 hasher->allocate_hash 返回值检查**:
```c
if (!hasher->allocate_hash(hasher, pub, fp))
{
    hasher->destroy(hasher);
    return FALSE;
}
```

4. **添加 refcount 成员**:
```c
struct private_mldsa_private_key_t {
    ...
    refcount_t ref;
    ...
};
```

**同时更新了核心类型定义**:

在 `/home/ipsec/strongswan/src/libstrongswan/credentials/keys/public_key.h`:
- 添加 `KEY_MLDSA65 = 1053` 到 `key_type_t` 枚举
- 添加 `SIGN_MLDSA65` 到 `signature_scheme_t` 枚举

在 `/home/ipsec/strongswan/src/libstrongswan/crypto/hashers/hasher.c`:
- 添加 `SIGN_MLDSA65` 到 `hasher_from_signature_scheme()` switch

在 `/home/ipsec/strongswan/src/libstrongswan/credentials/keys/public_key.c`:
- 添加 `SIGN_MLDSA65` 到 `signature_scheme_to_oid()` switch
- 添加 `SIGN_MLDSA65` 到 `key_type_from_signature_scheme()` switch

**修复文件**:
- `/home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/mldsa_private_key.c`
- `/home/ipsec/strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.c`
- `/home/ipsec/strongswan/src/libstrongswan/credentials/keys/public_key.h`
- `/home/ipsec/strongswan/src/libstrongswan/credentials/keys/public_key.c`
- `/home/ipsec/strongswan/src/libstrongswan/crypto/hashers/hasher.c`

**验证结果**:
- ✅ 编译成功
- ✅ 安装成功
- ✅ mldsa 插件正确加载

---

### 2026-03-02: ML-DSA 混合证书方案实现

**问题**: OpenSSL 3.0.2 不支持 ML-DSA 证书生成，无法生成标准的 ML-DSA X.509 证书

**解决方案**: 混合证书方案 - 在标准 X.509 证书的自定义扩展中存储 ML-DSA 公钥

**实现**:
1. **混合证书生成器** (`scripts/generate_mldsa_hybrid_cert.c`):
   - 生成 ML-DSA-65 密钥对 (liboqs)
   - 生成 ECDSA P-256 占位符密钥
   - 创建包含 ML-DSA 公钥扩展的 X.509 证书
   - 使用 ECDSA CA 签名证书

2. **mldsa 插件更新** (`mldsa_signer.c`):
   - 新增 `extract_mldsa_pubkey_from_cert()` - 从证书扩展提取 ML-DSA 公钥
   - `set_key()` 支持证书数据作为输入 (长度 > 500 bytes)

**证书结构**:
```
X.509 v3 证书
├── SubjectPublicKeyInfo: ECDSA P-256 (占位符)
├── 扩展:
│   ├── SAN: DNS:<name>.pqgm.test
│   ├── keyUsage: digitalSignature, keyEncipherment
│   └── 1.3.6.1.4.1.99999.1.2: ML-DSA-65 公钥 (1952 bytes)
└── 签名: ECDSA-SHA256 (CA 签名)
```

**OID 定义**:
```
OID: 1.3.6.1.4.1.99999.1.2
DER: 06 0A 2B 06 01 04 01 86 8D 1F 01 02
```

**修复的 Bug**:
1. OID DER 编码错误: `87 6F 0F` → `86 8D 1F` (base-128 编码)
2. 重复的 mldsa_oid 定义导致变量遮蔽
3. 测试文件中 TRUE/FALSE 未定义

**验证结果**:
- ✅ 混合证书生成成功
- ✅ ML-DSA 公钥从证书扩展提取成功
- ✅ 签名/验证测试通过 (7/7)
- ⏳ IKE_AUTH 集成测试待进行

**生成的文件**:
- `docker/initiator/certs/mldsa/initiator_hybrid_cert.pem`
- `docker/initiator/certs/mldsa/initiator_mldsa_key.bin` (4032 bytes)
- `docker/responder/certs/mldsa/responder_hybrid_cert.pem`
- `docker/responder/certs/mldsa/responder_mldsa_key.bin` (4032 bytes)

**详细记录**: [MLDSA-HYBRID-CERT-SUMMARY.md](MLDSA-HYBRID-CERT-SUMMARY.md)

**Git 提交**: `84bdd30 feat(mldsa): implement ML-DSA public key extraction from certificate extension`

---

### 2026-03-02: ML-DSA 私钥加载器实现

**问题**: strongSwan 无法加载原始 ML-DSA 二进制私钥文件 (4032 bytes)

**解决方案**: 实现 PRIVKEY builder 模式的私钥加载器

**实现**:
1. **mldsa_private_key.c/h**: 实现私钥加载器
   - 使用 `PLUGIN_REGISTER(PRIVKEY, ...)` 注册
   - 支持 `BUILD_BLOB`、`BUILD_BLOB_PEM`、`BUILD_FROM_FILE` builder parts
   - 使用 `chunk_map()` 读取文件
   - 实现 `private_key_t` 接口

2. **关键修复**:
   - 添加 `__attribute__((visibility("default")))` 导出函数符号
   - 正确处理 `BUILD_BLOB_PEM` 类型 (type=5)
   - 修复 rpath 问题 (`chrpath -r /usr/local/lib`)

**验证结果**:
- ✅ ML-DSA 私钥成功加载 (4032 bytes)
- ✅ 日志显示: `loaded private key from '.../initiator_mldsa_key.bin'`
- ✅ 日志显示: `ML-DSA: loaded private key successfully`
- ✅ 从 5 builders 增加到 7 builders

**Git 提交**: `32e31b3 feat(mldsa): implement ML-DSA private key loader with builder pattern`

**详细实现思路**: [2026-03-02-mldsa-ike-auth-implementation-notes.md](plans/2026-03-02-mldsa-ike-auth-implementation-notes.md)

**待完成**:
- IKE 提案配置调试
- ML-DSA IKE_AUTH 认证集成 (需要修改认证流程使用 ML-DSA 而不是 ECDSA)

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
| ML-DSA私钥加载器 | `mldsa_private_key.c/h` |
| ML-DSA插件注册 | `mldsa_plugin.c` |
| ML-DSA证书生成 | `scripts/generate_mldsa_*.c` |
| ML-DSA混合证书生成 | `scripts/generate_mldsa_hybrid_cert.c` |
