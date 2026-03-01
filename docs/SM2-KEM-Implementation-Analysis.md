# SM2-KEM 实现问题分析与改进方案

## 概述

本文档分析当前 SM2-KEM 实现中发现的 4 个关键问题，并提出改进方案。

---

## 问题 1：SM2-KEM 性能异常（31.4ms）

### 现象

| RTT | 阶段 | 延迟 |
|-----|------|------|
| RTT 2 | IKE_INTERMEDIATE #0 (证书交换) | 1.8 ms |
| **RTT 3** | **IKE_INTERMEDIATE #1 (SM2-KEM)** | **31.4 ms** |
| RTT 4 | IKE_INTERMEDIATE #2 (ML-KEM-768) | 2.6 ms |

SM2-KEM 比证书交换慢 **17 倍**，这不合理。SM2 加解密本身应该很快（< 5ms）。

### 原因分析

查看 `gmalg_ke.c` 代码，发现以下性能问题：

1. **大量调试输出**：
   ```c
   fprintf(stderr, "DEBUG initiator_encaps: my_random (r_i) = ");
   for (int i = 0; i < 8; i++) fprintf(stderr, "%02x", ...);
   fprintf(stderr, "...\n");
   fflush(stderr);  // 每次都刷新缓冲区
   ```
   这些 `fprintf` 和 `fflush` 在生产环境中会严重拖慢性能。

2. **文件 I/O 操作**：
   ```c
   if (load_sm2_pubkey_from_file(SM2_PEER_PUBKEY_FILE, &sm2_peer_key) == 1)
   ```
   每次加解密都可能触发文件读取操作。

3. **私钥解密操作**：
   ```c
   sm2_private_key_info_decrypt_from_pem(sm2_key, SM2_PRIVKEY_PASSWORD, fp);
   ```
   加载加密的私钥文件需要解密操作。

### 性能对比参考

根据 GmSSL 官方数据：
- SM2 签名：~3ms（256位）
- SM2 加密：~5ms（典型数据）
- SM2 解密：~3ms

**预期 SM2-KEM（1次加密+1次解密）应该在 10ms 以内**，而不是 31.4ms。

### 改进方案

1. **移除调试输出**：将所有 `fprintf(stderr, ...)` 替换为 `DBG1()` 宏，仅在调试模式输出
2. **预加载密钥**：在 IKE_SA 创建时加载密钥，缓存到内存
3. **避免文件 I/O**：从证书中直接提取公钥，不使用文件 fallback

---

## 问题 2：公钥获取方式错误

### 现象

当前实现需要手动复制对端公钥文件：
```bash
# 发起方需要响应方的公钥
sudo cp docker/responder/certs/x509/peer_sm2_pubkey.pem /usr/local/etc/swanctl/x509/
# 响应方需要发起方的公钥
sudo cp docker/initiator/certs/x509/peer_sm2_pubkey.pem /usr/local/etc/swanctl/x509/
```

**这是错误的！** 应该从 IKE_INTERMEDIATE #1 收到的 EncCert 中自动提取。

### 参考文档设计（正确流程）

```
IKE_INTERMEDIATE #1：双证书分发（不更新密钥）
- 发送：I -> R：SK { CERT(SignCert_i), CERT(EncCert_i), [CERTREQ] }
- 发送：R -> I：SK { CERT(SignCert_r), CERT(EncCert_r) }

交互完成后：
- I 从 EncCert_r 中提取 R 的加密公钥
- R 从 EncCert_i 中提取 I 的加密公钥
- 存储并流转给 SM2-KEM 使用
```

### 当前实现分析

在 `ike_cert_post.c` 中已有 `process_sm2_certs()` 函数：

```c
static void process_sm2_certs(private_ike_cert_post_t *this, message_t *message)
{
    // ... 遍历收到的证书
    // 尝试提取 SM2 公钥
    if (x509_key.algor == 17 ||  /* OID_sm2 */
        x509_key.algor == 301) {
        memcpy(&sm2_pubkey, &x509_key.u.sm2_key, sizeof(SM2_KEY));
        // 存储全局变量
        gmalg_set_peer_sm2_pubkey(&sm2_pubkey);
    }
}
```

**问题**：
1. `x509_key.algor` 的值可能不正确（应该是 SM2 OID: 1.2.156.10197.1.301）
2. 需要区分 SignCert 和 EncCert - 只有 EncCert 才用于 SM2-KEM
3. `gmalg_set_peer_sm2_pubkey()` 设置的全局变量可能未被 `gmalg_ke.c` 正确使用

### 改进方案

1. **增强证书解析**：正确识别 EncCert（通过 KeyUsage: keyEncipherment）
2. **修复公钥传递**：确保 `gmalg_ke.c` 优先使用从证书提取的公钥
3. **移除文件 fallback**：删除 `peer_sm2_pubkey.pem` 的依赖

```c
// 正确的流程：
// 1. IKE_INTERMEDIATE #1 收到 EncCert
// 2. process_sm2_certs() 提取公钥
// 3. 存储到 IKE_SA 上下文（不是全局变量）
// 4. IKE_INTERMEDIATE #2 创建 SM2-KEM 时注入公钥
```

---

## 问题 3：SM2 双证书机制理解

### 正确理解

国密 SM2 双证书机制（GM/T 0009-2012）：

| 证书类型 | 用途 | KeyUsage | KeyPair |
|----------|------|----------|---------|
| **签名证书 (SignCert)** | 身份认证、数字签名 | digitalSignature, nonRepudiation | 签名公钥 + 签名私钥 |
| **加密证书 (EncCert)** | 密钥加密、数据加密 | keyEncipherment, dataEncipherment | 加密公钥 + 加密私钥 |

**关键点**：
- 两套密钥对是**独立的**，不能混用
- SignCert 的私钥用于签名，公钥用于验签
- **EncCert 的公钥用于加密（SM2-KEM封装），私钥用于解密（SM2-KEM解封装）**

### 当前实现问题

1. **密钥混用风险**：
   - 代码中没有明确区分 SignCert 和 EncCert
   - 可能错误使用签名密钥进行加密操作

2. **证书加载顺序**：
   ```c
   // 当前代码同时加载两个证书
   add_cert_from_file(this, sign_cert_path, "SignCert", message);
   add_cert_from_file(this, enc_cert_path, "EncCert", message);
   ```
   但在提取公钥时，需要确保提取的是 EncCert 的公钥。

### 验证方法

检查证书的 KeyUsage：
```bash
# 查看 SignCert
openssl x509 -in sm2_sign_cert.pem -text -noout | grep -A1 "Key Usage"

# 查看 EncCert
openssl x509 -in sm2_enc_cert.pem -text -noout | grep -A1 "Key Usage"
```

预期输出：
- SignCert: `Digital Signature, Non Repudiation`
- EncCert: `Key Encipherment, Data Encipherment`

### 改进方案

1. **证书识别**：通过 KeyUsage 区分 SignCert 和 EncCert
2. **密钥隔离**：确保 SM2-KEM 只使用 EncCert 的密钥对
3. **代码注释**：明确标注每种证书的用途

---

## 问题 4：与参考文档的差距分析

### 参考文档：5-RTT 协议流程

```
1. IKE_SA_INIT
   - 协商 SA 与 DH 密钥 SK
   - [当前实现] ✅ 已实现

2. IKE_INTERMEDIATE #1：双证书分发
   - 交换 SignCert + EncCert
   - 从 EncCert 提取加密公钥
   - [约束] 不携带 KE，不更新密钥
   - [当前实现] ⚠️ 部分实现（证书交换OK，公钥提取有问题）

3. IKE_INTERMEDIATE #2：SM2-KEM
   - 双向封装：I 发送 ct_i，R 发送 ct_r
   - SK = r_i || r_r（64字节）
   - [约束] 完成后按 RFC 9370 更新密钥
   - [当前实现] ⚠️ SM2-KEM 工作但密钥更新未验证

4. IKE_INTERMEDIATE #3：ML-KEM-768
   - 标准 ML-KEM 封装
   - [约束] 使用 SM2-KEM 更新后的密钥材料
   - [约束] 完成后按 RFC 9370 更新密钥
   - [当前实现] ⚠️ ML-KEM 工作但密钥更新链未验证

5. IKE_AUTH
   - 发送 AUTH 证书和签名
   - [约束] 必须将所有 intermediate 内容纳入 AUTH 绑定（RFC 9242 IntAuth）
   - [当前实现] ❌ 使用 PSK 而非后量子签名证书
```

### 差距汇总

| 功能 | 参考文档要求 | 当前实现 | 差距 |
|------|--------------|----------|------|
| IKE_SA_INIT | RFC 9370 多重密钥交换 | ✅ 已实现 | 无 |
| IKE_INTERMEDIATE #1 证书交换 | SignCert + EncCert | ✅ 已实现 | 无 |
| IKE_INTERMEDIATE #1 公钥提取 | 从 EncCert 自动提取 | ❌ 使用文件 fallback | **需修复** |
| IKE_INTERMEDIATE #2 SM2-KEM | 双向封装 + RFC 9370 更新 | ⚠️ 封装OK，更新未验证 | 需验证 |
| IKE_INTERMEDIATE #3 ML-KEM | RFC 9370 密钥更新链 | ⚠️ 工作但链未验证 | 需验证 |
| IKE_AUTH IntAuth 绑定 | RFC 9242 中间内容绑定 | ❌ 未实现 | **需实现** |
| IKE_AUTH 认证方式 | 后量子签名 (ML-DSA) | ❌ 使用 PSK | **需改进** |

### 关键差距详解

#### 4.1 公钥提取机制（需修复）

**参考文档**：
> I 从 EncCert_r 中提取 R 的加密公钥，R 从 EncCert_i 中提取 I 的加密公钥

**当前实现**：
```c
// gmalg_ke.c 使用文件 fallback
if (load_sm2_pubkey_from_file(SM2_PEER_PUBKEY_FILE, &sm2_peer_key) == 1)
```

**改进**：
- 在 `ike_cert_post.c` 的 `process_sm2_certs()` 中正确提取公钥
- 通过 IKE_SA 上下文传递给 SM2-KEM 实例

#### 4.2 RFC 9370 密钥更新链（需验证）

**参考文档**：
> SM2-KEM 完成后按 RFC 9370 更新密钥，得到 SKEYSEED(1)、SK_*(1)
> ML-KEM 完成后按 RFC 9370 更新密钥，得到 SKEYSEED(2)、SK_*(2)

**当前实现**：strongSwan 的 `ike_init.c` 应该已经实现了 RFC 9370，但需要验证：
- SM2-KEM 的共享密钥是否正确参与了密钥派生
- ML-KEM 是否使用了 SM2-KEM 更新后的密钥材料

#### 4.3 IntAuth 绑定（需实现）

**参考文档**：
> 必须将所有 intermediate 的关键内容纳入 AUTH 绑定（按 RFC 9242 的 IntAuth 思路）

**当前实现**：未实现。IKE_AUTH 阶段没有将 IKE_INTERMEDIATE 的内容纳入 AUTH 计算。

**风险**：中间人攻击者可以篡改 IKE_INTERMEDIATE 的内容（如替换证书），而 AUTH 验证不会失败。

---

## 改进优先级

| 优先级 | 问题 | 影响 | 工作量 |
|--------|------|------|--------|
| **P0** | 公钥提取机制 | 安全性 + 功能正确性 | 中 |
| **P1** | SM2-KEM 性能优化 | 用户体验 | 低 |
| **P2** | IntAuth 绑定 | 安全性 | 高 |
| **P3** | 后量子签名认证 | 完整性 | 高 |
| **P4** | RFC 9370 密钥更新链验证 | 正确性验证 | 中 |

---

## 下一步行动

1. **立即修复**：公钥提取机制
   - 修改 `process_sm2_certs()` 正确识别 EncCert
   - 确保公钥传递到 SM2-KEM 实例
   - 移除文件 fallback 依赖

2. **性能优化**：移除调试输出
   - 将 `fprintf(stderr, ...)` 改为 `DBG1()`

3. **后续实现**：IntAuth 绑定
   - 研究 RFC 9242 的 IntAuth 机制
   - 修改 AUTH 计算包含 intermediate 内容

---

*文档创建时间: 2026-03-01*
*作者: Claude Code AI Assistant*
