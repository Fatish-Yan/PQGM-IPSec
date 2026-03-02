# PQ-GM-IKEv2 BUG记录

> **重要**: 遇到任何BUG时，先查阅此文档！避免重复犯错！

---

## 已知BUG历史

### BUG-001: SM2 EncCert OID检查条件错误 (重复发生!)

**状态**: ✅ 已修复 (但曾因记忆丢失重复发生)

**发现时间**: 2026-03-02 (实际上P0修复时应该已解决)

**症状**:
- Responder日志: `PQ-GM-IKEv2: EncCert key is not SM2 (algor=18)`
- Responder返回空IKE_INTERMEDIATE响应
- SM2-KEM双向交换失败

**根因**:
`ike_cert_post.c` 中的OID检查条件错误：
```c
// 错误
if (x509_key.algor == 17 || x509_key.algor == 19)

// 正确
if (x509_key.algor == 18 && x509_key.algor_param == 5)
```

**GmSSL 3.1.3 正确OID值**:
- `OID_ec_public_key = 18` (算法)
- `OID_sm2 = 5` (曲线参数)

**教训**:
- 修复时必须记录到文档
- 遇到类似问题先查文档
- **此BUG因上下文压缩导致记忆丢失而重复发生**

---

### BUG-002: SM2证书无法被OpenSSL解析

**状态**: ✅ 已知限制 (不影响功能)

**症状**:
- `loading '/usr/local/etc/swanctl/x509/encCert.pem' failed: parsing X509 certificate failed`
- `building CRED_CERTIFICATE - X509 failed, tried 4 builders`

**原因**:
- strongSwan使用OpenSSL解析证书
- OpenSSL 3.0.2不完全支持GmSSL生成的SM2证书签名算法

**解决方案**:
- 代码绕过strongSwan的证书管理器，直接从文件读取证书
- 使用GmSSL函数解析SM2证书

---

### BUG-003: 私钥文件名不匹配

**状态**: ✅ 已修复

**症状**:
- `SM2-KEM: load_sm2_privkey_from_file: cannot open /usr/local/etc/swanctl/private/sm2_enc_key.pem`

**原因**:
- 代码硬编码私钥文件名为 `sm2_enc_key.pem`
- 但实际文件名可能是 `encKey.pem`

**解决方案**:
- 统一使用 `sm2_enc_key.pem` 作为SM2加密私钥文件名
- 或修改代码中的路径定义

**文件路径定义位置**: `gmalg_ke.c`
```c
#define SM2_MY_PRIVKEY_FILE "/usr/local/etc/swanctl/private/sm2_enc_key.pem"
```

---

## 调试技巧

### 1. 检查证书OID
```bash
gmssl certparse -in /path/to/cert.pem
```
查看 `algor` 和 `algor_param` 值

### 2. 检查证书KeyUsage
```bash
gmssl certparse -in /path/to/cert.pem | grep -A5 "KeyUsage"
```
EncCert必须有 `keyEncipherment`

### 3. 启用详细日志
在 `strongswan.conf` 中:
```
filelog {
    stdout {
        default = 1
        ike = 3
        cfg = 2
    }
}
```

### 4. 检查GmSSL符号
```bash
nm -D /usr/local/lib/libgmssl.so.3 | grep x509_cert
```

---

## 常见错误信息速查

| 错误信息 | 含义 | 解决方案 |
|---------|------|---------|
| `EncCert key is not SM2 (algor=18)` | OID检查条件错误 | 见BUG-001 |
| `certificate is not EncCert (no keyEncipherment)` | 证书没有KeyUsage:keyEncipherment | 使用正确的EncCert |
| `cannot open certificate file` | 文件路径错误 | 检查文件是否存在 |
| `sm2_encrypt failed` | SM2加密失败 | 检查公钥是否正确加载 |
| `sm2_decrypt_data failed` | SM2解密失败 | 检查私钥是否正确加载 |
| `compute_shared_secret missing randoms` | 缺少随机数 | 检查KE流程是否完整 |

---

### BUG-004: ML-DSA 签名验证失败 - 混合证书公钥不匹配

**状态**: ❌ 未修复 (当前阻塞问题)

**发现时间**: 2026-03-02

**症状**:
```
[LIB] ML-DSA: signature created successfully, len=3309
[IKE] authentication of 'initiator.pqgm.test' (myself) with (23) failed
```

**根因**:
1. Initiator 成功生成 ML-DSA 签名 (3309 bytes)
2. Responder 收到签名后，从证书提取公钥进行验证
3. **混合证书的 SubjectPublicKeyInfo 是 ECDSA P-256**
4. Responder 尝试用 ECDSA 公钥验证 ML-DSA 签名 → 验证失败

**流程分析**:
```
Initiator                           Responder
   |                                    |
   |  IKE_AUTH (ML-DSA signature)       |
   |----------------------------------->|
   |                                    | 1. 提取证书公钥 (ECDSA P-256)
   |                                    | 2. 尝试用 ECDSA 验证 ML-DSA 签名
   |                                    | 3. 验证失败 → AUTH_FAILED
   |<-----------------------------------|
   |        AUTH_FAILED                 |
```

**待完成工作**:
1. **实现 ML-DSA 公钥提取**: 从混合证书扩展 (OID 1.3.6.1.4.1.99999.1.2) 提取 ML-DSA 公钥
2. **实现 public_key_t 接口**: 添加 ML-DSA 公钥的 `verify()` 方法
3. **修改认证流程**: 识别混合证书，使用正确的公钥类型进行验证

**相关文件**:
- `src/libstrongswan/credentials/keys/public_key.c` - 添加 verify 方法
- `src/libcharon/sa/ikev2/tasks/ike_auth.c` - 认证流程修改
- `src/libstrongswan/plugins/mldsa/mldsa_public_key.c` - 新建公钥实现

**临时解决方案**:
- 无 (这是必须解决的架构问题)

---

## 文档更新记录

| 日期 | 更新内容 |
|------|---------|
| 2026-03-02 | 创建文档，记录BUG-001/002/003 |
| 2026-03-02 | 添加 BUG-004: ML-DSA 验证失败问题 |
