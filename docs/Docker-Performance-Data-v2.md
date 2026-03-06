# 5-RTT PQ-GM-IKEv2 Docker 性能测试报告 v2

## 测试环境

| 参数 | 发起端 | 响应端 |
|------|--------|--------|
| IP 地址 | 172.28.0.10 | 172.28.0.20 |
| 主机名 | initiator.pqgm.test | responder.pqgm.test |
| 平台 | Docker Ubuntu 22.04 | Docker Ubuntu 22.04 |
| strongSwan | 6.0.4 (修改版) | 6.0.4 (修改版) |
| GmSSL | 3.1.1 | 3.1.1 |
| 认证方式 | PSK | PSK |

## 代码修改 (v2)

### 1. ike_cert_post.c - EncCert KeyUsage 检查
- 添加 `x509_cert_check()` 检查证书用途
- 只提取 **EncCert** (keyEncipherment) 的公钥
- 区分 SignCert (digitalSignature) 和 EncCert (keyEncipherment)

### 2. gmalg_ke.c - 公钥获取优先级
```
Priority 1: g_peer_sm2_pubkey (从 IKE_INTERMEDIATE EncCert 提取)
Priority 2: this->peer_enccert (直接设置)
Priority 3: credmgr 查找
Priority 4: 文件 fallback (deprecated)
```

### 3. gmalg_ke.c - 性能优化
- 移除所有 `fprintf(stderr, ...)` 调试输出
- 移除所有 `fflush()` 调用
- 使用 `DBG1()` 宏替代调试输出

## 测试结果

### 总体性能

| 指标 | 值 |
|------|-----|
| **总握手时间** | ~75 ms |
| 总交换包数 | 12 对 (24 个) |
| IKE_SA 状态 | ✅ 建立成功 |
| CHILD_SA 状态 | ✅ 建立成功 |

### 5-RTT 详细分析

| RTT | 阶段 | 功能 | 延迟 |
|-----|------|------|------|
| 1 | IKE_SA_INIT | x25519 KE 协商 | ~1.7 ms |
| 2 | IKE_INTERMEDIATE #0 | SM2 双证书交换 | ~0.7 ms |
| 3 | IKE_INTERMEDIATE #1 | SM2-KEM 密钥交换 | ~33 ms |
| 4 | IKE_INTERMEDIATE #2 | ML-KEM-768 密钥交换 | ~1.8 ms |
| 5 | IKE_AUTH | PSK 认证 | ~2 ms |

### 日志分析

```
[IKE] initiating IKE_SA pqgm-ikev2[2] to 172.28.0.20
[CFG] selected proposal: IKE:AES_CBC_256/HMAC_SHA2_256_128/PRF_HMAC_SHA2_256/CURVE_25519/KE1_(1051)/KE2_ML_KEM_768

[IKE] PQ-GM-IKEv2: loading SM2 certificates from files for IKE_INTERMEDIATE
[IKE] PQ-GM-IKEv2: sending SignCert certificate (382 bytes DER)
[IKE] PQ-GM-IKEv2: sending EncCert certificate (392 bytes DER)

[IKE] SM2-KEM: created instance, is_initiator=1
[IKE] SM2-KEM: WARNING - using file fallback (deprecated)
[IKE] SM2-KEM: loaded peer pubkey from file /usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem
[IKE] SM2-KEM: returning ciphertext of 139 bytes

[IKE] SM2-KEM: set_public_key called with 141 bytes
[IKE] SM2-KEM: WARNING - using file fallback for private key (deprecated)
[IKE] SM2-KEM: loaded private key from file /usr/local/etc/swanctl/private/sm2_enc_key.pem
[IKE] SM2-KEM: decrypted peer_random
[IKE] SM2-KEM: computed shared secret (64 bytes)

[IKE] IKE_SA pqgm-ikev2[2] established
[IKE] CHILD_SA ipsec{1} established with SPIs c9ee8b59_i c4c8d30a_o
```

## 已知问题

### 1. 证书公钥提取 (待完善)

**问题**: `cert_payload->get_cert()` 无法解析 SM2 证书

```
03[IKE] PQ-GM-IKEv2: could not get certificate from payload
03[LIB] building CRED_CERTIFICATE - X509 failed, tried 4 builders
```

**原因**: strongSwan 默认的证书解析器不支持 SM2 证书（需要 GmSSL 解析器）

**当前状态**: 使用文件 fallback 机制可以正常工作

**解决方案** (后续): 实现 SM2 证书的 strongSwan 解析器插件

### 2. SM2-KEM 性能 (待优化)

**现象**: SM2-KEM 步骤延迟 ~33ms

**可能原因**:
- 每次从加密的 PEM 文件加载私钥
- GmSSL SM2 操作本身的性能

**优化方向**:
- 预加载私钥到内存
- 使用 GmSSL 的优化版本

## 实现状态

| 功能 | 状态 | 备注 |
|------|------|------|
| IKE_SA_INIT + RFC 9370 | ✅ 工作 | x25519 + KE1 + KE2 提案 |
| IKE_INTERMEDIATE #0 证书交换 | ✅ 工作 | SignCert + EncCert |
| IKE_INTERMEDIATE #1 SM2-KEM | ✅ 工作 | 使用文件 fallback |
| IKE_INTERMEDIATE #2 ML-KEM | ✅ 工作 | 标准实现 |
| IKE_AUTH PSK | ✅ 工作 | |
| EncCert 公钥提取 | ⚠️ 部分 | 代码已实现，证书解析待完善 |
| 性能优化 | ⚠️ 部分 | 移除了调试输出，私钥加载待优化 |

## 结论

1. **功能测试**: ✅ 通过 - 5-RTT PQ-GM-IKEv2 完整流程成功
2. **性能改进**: 部分改进 - 移除了调试输出，但 SM2-KEM 仍有优化空间
3. **公钥提取**: 代码已实现，待 strongSwan SM2 证书解析器支持

---

*测试时间: 2026-03-01*
*测试平台: Docker Ubuntu 22.04*
