# RFC 9370 密钥更新链验证设计

## 概述

本文档描述 PQ-GM-IKEv2 实现中 RFC 9370 密钥更新链的验证方案设计。

**目标**: 验证 SM2-KEM 和 ML-KEM 的共享秘密正确参与密钥派生，并生成可用于论文的证明数据。

**日期**: 2026-03-01

---

## 1. 验证目标

### 1.1 密钥更新链验证

| 阶段 | 验证内容 | 预期结果 |
|------|----------|----------|
| IKE_SA_INIT | SKEYSEED(0) 和 SK_*(0) 派生 | 只使用 x25519 |
| IKE_INTERMEDIATE #0 | 无 KE，密钥不变 | SK_*(0) 保持不变 |
| IKE_INTERMEDIATE #1 | SM2-KEM 密钥更新 | SKEYSEED(1) = prf(SK_d(0), sm2kem_ss \| Ni \| Nr) |
| IKE_INTERMEDIATE #2 | ML-KEM 密钥更新 | SKEYSEED(2) = prf(SK_d(1), mlkem_ss \| Ni \| Nr) |
| IKE_AUTH | AUTH 使用最终密钥 | 使用 SK_pi(2)/SK_pr(2) |

### 1.2 验证方法

采用 **方案 A + B 组合**：
1. **日志增强**：在关键函数中添加调试日志
2. **密钥哈希链**：输出每轮 SK_* 密钥的 SHA256 哈希

---

## 2. RFC 9370 密钥派生公式（参考）

### 2.1 初始 IKE_SA_INIT

```
SKEYSEED(0) = prf(Ni | Nr, g^ir)

{SK_d(0) | SK_ai(0) | SK_ar(0) | SK_ei(0) | SK_er(0) | SK_pi(0) | SK_pr(0)}
    = prf+(SKEYSEED(0), Ni | Nr | SPIi | SPIr)
```

### 2.2 每个 IKE_INTERMEDIATE 后

```
SKEYSEED(n) = prf(SK_d(n-1), SK(n) | Ni | Nr)

{SK_d(n) | SK_ai(n) | SK_ar(n) | SK_ei(n) | SK_er(n) | SK_pi(n) | SK_pr(n)}
    = prf+(SKEYSEED(n), Ni | Nr | SPIi | SPIr)
```

**关键要点**：
- `SK_d(n-1)` 是**上一轮**的 SK_d（链式更新）
- `SK(n)` 是当前 KE 的共享秘密
- Ni, Nr 始终是 IKE_SA_INIT 的 nonces

---

## 3. 实现方案

### 3.1 修改文件

| 文件 | 修改内容 |
|------|----------|
| `keymat_v2.c` | 添加密钥派生日志 |
| `ike_init.c` | 添加 KE 完成日志 |

### 3.2 keymat_v2.c 修改

在 `derive_ike_keys()` 函数中添加日志：

```c
// 在 SKEYSEED 计算后
DBG1(DBG_IKE, "RFC 9370 Key Derivation:");
DBG1(DBG_IKE, "  SKEYSEED = prf(%s, SK(%d) | Ni | Nr)",
     n == 0 ? "Ni|Nr" : "SK_d(n-1)", n);
DBG1(DBG_IKE, "  SK(%d) shared secret: %zu bytes", n, secret.len);

// 在 SK_* 派生后
DBG1(DBG_IKE, "  Derived keys (SHA256 hashes):");
DBG1(DBG_IKE, "    SK_d  = %.*s", 16, hash_sha256(sk_d));
DBG1(DBG_IKE, "    SK_pi = %.*s, SK_pr = %.*s", 16, hash_sha256(sk_pi), 16, hash_sha256(sk_pr));
```

### 3.3 ike_init.c 修改

在 `key_exchange_done()` 函数中添加日志：

```c
DBG1(DBG_IKE, "RFC 9370: IKE_INTERMEDIATE #%d KE completed", ke_index);
if (this->ke && this->ke->get_shared_secret)
{
    chunk_t ss;
    if (this->ke->get_shared_secret(this->ke, &ss))
    {
        DBG1(DBG_IKE, "  KE shared secret: %zu bytes", ss.len);
        chunk_clear(&ss);
    }
}
```

---

## 4. 预期日志输出

```
[IKE] RFC 9370 Key Derivation Chain:
[IKE]
[IKE] IKE_SA_INIT:
[IKE]   SKEYSEED(0) = prf(Ni|Nr, x25519_ss)
[IKE]   SK_d(0)  = a1b2c3d4e5f6...
[IKE]   SK_pi(0) = 1234567890ab..., SK_pr(0) = fedcba098765...
[IKE]
[IKE] IKE_INTERMEDIATE #0: No KE, keys unchanged
[IKE]
[IKE] IKE_INTERMEDIATE #1: SM2-KEM
[IKE]   SM2-KEM shared secret: 64 bytes
[IKE]   SKEYSEED(1) = prf(SK_d(0), sm2kem_ss | Ni | Nr)
[IKE]   SK_d(1)  = 9876543210ab...  [CHANGED]
[IKE]   SK_pi(1) = abcdef123456..., SK_pr(1) = 654321fedcba...
[IKE]
[IKE] IKE_INTERMEDIATE #2: ML-KEM-768
[IKE]   ML-KEM shared secret: 32 bytes
[IKE]   SKEYSEED(2) = prf(SK_d(1), mlkem_ss | Ni | Nr)
[IKE]   SK_d(2)  = 0a1b2c3d4e5f...  [CHANGED]
[IKE]   SK_pi(2) = f0e1d2c3b4a5..., SK_pr(2) = 5a4b3c2d1e0f...
[IKE]
[IKE] IKE_AUTH: Using SK_pi(2)/SK_pr(2) for AUTH calculation
```

---

## 5. 验证检查点

### 5.1 密钥更新验证

- [ ] SK_d(1) != SK_d(0)：证明 SM2-KEM 后密钥已更新
- [ ] SK_d(2) != SK_d(1)：证明 ML-KEM 后密钥已更新
- [ ] SK_pi(2)/SK_pr(2) 用于 AUTH：证明最终密钥正确使用

### 5.2 共享秘密验证

- [ ] SM2-KEM 共享秘密长度 = 64 字节
- [ ] ML-KEM 共享秘密长度 = 32 字节
- [ ] 双方计算相同的共享秘密

### 5.3 IntAuth 验证（后续）

- [ ] AUTH 计算包含 IKE_INTERMEDIATE 内容

---

## 6. 论文数据展示

### 6.1 密钥更新链图示

```
IKE_SA_INIT          IKE_INT #1           IKE_INT #2           IKE_AUTH
    |                    |                    |                    |
    v                    v                    v                    v
x25519_ss           sm2kem_ss            mlkem_ss               AUTH
    |                    |                    |                    |
    v                    v                    v                    v
SKEYSEED(0)  --->  SKEYSEED(1)  --->   SKEYSEED(2)  --->    SK_pi(2)/SK_pr(2)
    |                    |                    |
    v                    v                    v
  SK_*(0)             SK_*(1)              SK_*(2)
```

### 6.2 密钥哈希对比表

| 阶段 | SK_d 哈希 (前16字节) | SK_pi 哈希 (前16字节) | 变化 |
|------|---------------------|----------------------|------|
| IKE_SA_INIT | a1b2c3d4e5f6... | 1234567890ab... | - |
| IKE_INT #1 | 9876543210ab... | abcdef123456... | ✅ |
| IKE_INT #2 | 0a1b2c3d4e5f... | f0e1d2c3b4a5... | ✅ |

---

## 7. 实现步骤

1. **添加密钥派生日志** (keymat_v2.c)
   - 在 derive_ike_keys() 中添加详细日志
   - 输出 SK_* 密钥的 SHA256 哈希

2. **添加 KE 完成日志** (ike_init.c)
   - 在 key_exchange_done() 中添加日志
   - 输出共享秘密长度

3. **编译和部署**
   - 重新编译 strongSwan
   - 部署到测试环境

4. **运行测试**
   - 执行 5-RTT PQ-GM-IKEv2 握手
   - 收集日志

5. **分析日志**
   - 验证密钥更新链
   - 生成论文数据

---

## 8. 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| 日志泄露敏感信息 | 仅在测试环境使用，输出哈希而非原始密钥 |
| 日志影响性能 | 使用 DBG1 级别，可在生产环境禁用 |
| 密钥派生错误 | 对比 RFC 9370 附录示例 |

---

*文档创建时间: 2026-03-01*
*作者: Claude Code AI Assistant*
