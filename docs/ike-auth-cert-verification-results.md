# IKE_AUTH 证书认证 + IntAuth 绑定验证结果

## 测试环境
- **日期**: 2026-03-02
- **strongSwan**: 6.0.4 (修改版)
- **GmSSL**: 3.1.1
- **测试场景**: 5-RTT PQ-GM-IKEv2 with ECDSA Certificate Authentication
- **平台**: Docker Ubuntu 22.04

## 测试目标

验证 IKE_AUTH 阶段的 ECDSA 证书认证与 RFC 9242 IntAuth 绑定机制的正确性。

**注意**：
- IKE_INTERMEDIATE 阶段使用 SM2 双证书（signCert/encCert）用于 SM2-KEM
- IKE_AUTH 阶段使用独立的 ECDSA authCert 用于身份认证
- 两套证书是独立的，互不影响

## 修复的问题

### 问题 1: SM2-KEM 私钥加载与 ECDSA 证书冲突

**现象**：
```
SM2-KEM: failed to parse SM2 private key from DER
```

**原因**：
- 代码尝试从 credential manager 获取 ECDSA 私钥
- 在证书认证模式下，找到的是 ECDSA P-256 私钥（用于 IKE_AUTH）
- 尝试将其解析为 SM2 格式失败

**修复**：
在 `gmalg_ke.c` 中，当 Priority 3（ECDSA 格式解析）失败时，继续尝试 Priority 4（文件 fallback）：

```c
/* Priority 3 解析失败时，尝试文件 fallback */
if (sm2_private_key_info_from_der(&sm2_my_key, ...) != 1)
{
    DBG1(DBG_IKE, "SM2-KEM: ECDSA key is not SM2 format, trying file fallback");
    goto try_file_fallback;
}
```

### 问题 2: 证书缺少 SAN 导致验证失败

**现象**：
```
no trusted ECDSA public key found for 'initiator.pqgm.test'
```

**原因**：
- 生成的 ECDSA 证书没有 Subject Alternative Name (SAN)
- strongSwan 要求远程 ID 必须匹配证书的 SAN 或 DN

**修复**：
重新生成带 SAN 的 ECDSA 证书：
```bash
# 添加 SAN 扩展
openssl x509 -req ... -extfile san.cnf -extensions v3_req
```

## 测试结果

### 协议流程验证

```
IKE_SA_INIT:
  ✅ 协商提案: aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768
  ✅ RFC 9370 初始密钥派生

IKE_INTERMEDIATE #0 (证书交换):
  ✅ 发送 SM2 SignCert + EncCert
  ✅ 从 EncCert 提取 SM2 公钥
  ✅ IntAuth 链式更新

IKE_INTERMEDIATE #1 (SM2-KEM):
  ✅ SM2-KEM 封装 (140 bytes ciphertext)
  ✅ SM2 私钥从文件加载 (fallback)
  ✅ 共享密钥计算 (64 bytes)
  ✅ RFC 9370 密钥更新

IKE_INTERMEDIATE #2 (ML-KEM-768):
  ✅ ML-KEM 封装
  ✅ RFC 9370 密钥更新

IKE_AUTH:
  ✅ ECDSA 证书发送和验证
  ✅ CA 证书链验证
  ✅ IntAuth 绑定验证
  ✅ AUTH 签名验证

CHILD_SA:
  ✅ ESP: AES_GCM_16-256
  ✅ TUNNEL 模式建立
```

### 关键日志

```
Initiator:
[IKE] authentication of 'initiator.pqgm.test' (myself) with ECDSA_WITH_SHA256_DER successful
[IKE] IKE_SA pqgm-ikev2-cert[1] established between 172.28.0.10[initiator.pqgm.test]...172.28.0.20[responder.pqgm.test]
[IKE] CHILD_SA ipsec{1} established with SPIs c20756cf_i c93b95c8_o and TS 10.1.0.0/16 === 10.2.0.0/16

Responder:
[IKE] authentication of 'responder.pqgm.test' (myself) with ECDSA_WITH_SHA256_DER successful
[CFG]   using trusted certificate "C=CN, ST=Beijing, L=Beijing, O=PQGM-Test, CN=responder.pqgm.test"
[CFG]   using trusted ca certificate "C=CN, ST=Beijing, L=Beijing, O=PQGM-Test, CN=PQGM-Test-CA"
```

### SA 状态

```
Initiator:
pqgm-ikev2-cert: #1, ESTABLISHED, IKEv2
  local  'initiator.pqgm.test' @ 172.28.0.10[4500]
  remote 'responder.pqgm.test' @ 172.28.0.20[4500]
  AES_CBC-256/HMAC_SHA2_256_128/PRF_HMAC_SHA2_256/CURVE_25519/KE1_(1051)/KE2_ML_KEM_768
  ipsec: #1, INSTALLED, TUNNEL, ESP:AES_GCM_16-256
    local  10.1.0.0/16
    remote 10.2.0.0/16

Responder:
pqgm-ikev2-cert: #1, ESTABLISHED, IKEv2
  local  'responder.pqgm.test' @ 172.28.0.20[4500]
  remote 'initiator.pqgm.test' @ 172.28.0.10[4500]
```

## IntAuth 绑定验证

AUTH 计算公式 (RFC 9242)：
```
octets = message + nonce + prf(SK_px, IDx') + IntAuth
```

日志验证：
```
[IKE] octets = message + nonce + prf(Sk_px, IDx') + IntAuth => 396 bytes
```

- ✅ IntAuth 包含所有 IKE_INTERMEDIATE 消息的累积认证值
- ✅ AUTH 签名正确覆盖 IntAuth 数据
- ✅ 双向认证成功

## 验证检查点

| 检查项 | 状态 |
|--------|------|
| IKE_SA_INIT 成功 | ✅ |
| IKE_INTERMEDIATE #0 SM2 证书交换 | ✅ |
| IKE_INTERMEDIATE #1 SM2-KEM | ✅ |
| IKE_INTERMEDIATE #2 ML-KEM-768 | ✅ |
| RFC 9370 密钥更新链 | ✅ |
| RFC 9242 IntAuth 绑定 | ✅ |
| ECDSA 证书认证 | ✅ |
| CHILD_SA 建立 | ✅ |

## 证书说明

### SM2 双证书 (IKE_INTERMEDIATE 阶段)
- `signCert.pem`: SM2 签名证书 (身份标识)
- `encCert.pem`: SM2 加密证书 (SM2-KEM 公钥来源)

### ECDSA authCert (IKE_AUTH 阶段)
- `auth_ecdsa_cert.pem`: ECDSA P-256 证书 (身份认证)
- `auth_ca.pem`: CA 证书 (信任锚)
- 必须包含 SAN (Subject Alternative Name)

## 后续工作

1. **P3 后量子签名认证**: 将 ECDSA authCert 升级为 ML-DSA/SLH-DSA
2. **P1 性能优化**: 移除 SM2-KEM 调试输出
3. **P5 CERTREQ 规范化**: 低优先级

## 结论

**IKE_AUTH 证书认证 + IntAuth 绑定验证通过！**

1. ✅ ECDSA 证书认证成功
2. ✅ IntAuth 绑定正确工作
3. ✅ RFC 9242/9370 机制验证通过
4. ✅ SM2 双证书与 ECDSA authCert 独立工作
5. ✅ 5-RTT PQ-GM-IKEv2 完整流程验证通过

---

*测试时间: 2026-03-02*
*测试人员: Claude Code AI Assistant*
