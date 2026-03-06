# BUG: IKE_INTERMEDIATE 消息生成成功但未发送

## ✅ 已完全解决！（2026-03-04）

**最终状态**: GM对称栈 (SM4-CBC + HMAC-SM3-128 + PRF-SM3) 的IKE连接完全成功！

```
✅ IKE_SA_INIT    - SM4-CBC + HMAC-SM3-128 + PRF-SM3 提案协商成功
✅ IKE_INTERMEDIATE #0 - 证书交换成功
✅ IKE_INTERMEDIATE #1 - SM2-KEM 密钥交换成功
✅ IKE_INTERMEDIATE #2 - ML-KEM-768 密钥交换成功
✅ IKE_AUTH       - ML-DSA-65 签名认证成功

IKE_SA pqgm-5rtt-gm-symm[1] established!
CHILD_SA net{1} established!
initiate completed successfully!
```

---

## 问题概述

**环境**: Docker 容器测试，国密对称栈连接 (SM4-CBC + HMAC-SM3-128 + PRF-SM3)

**现象**: IKE_INTERMEDIATE #0（证书分发）消息已成功生成和加密，但没有被发送出去。

**影响**: 无法完成 5-RTT 握手流程，连接建立失败。

---

## 🚨 根本原因（2026-03-04 更新）

### 之前的分析是错误的！

> **警告**: 以下分析是基于日志表象的**错误推测**，已被外部 AI 分析纠正。
>
> ~~根本原因: `get_int_auth` 函数中 `skp_build` 为空~~
>
> **这是错误的！** `skp_build` 实际上不为空，日志中的 SK_pi/SK_pr hash 证明了密钥已正确派生。

### 真正的根本原因：PRF-SM3 缺少增量模式支持

**致命元凶**: `gmalg_hasher.c` 中的 PRF 实现没有支持 RFC 9242 IntAuth 计算所需的**增量模式**。

#### 问题分析

1. **RFC 9242 IntAuth 计算**: `keymat_v2.c` 使用增量式PRF调用来计算IntAuth：
   ```c
   // 第一次调用：缓存数据（bytes = NULL）
   prf->allocate_bytes(prf, prev, NULL);
   // 第二次调用：输出最终结果
   prf->allocate_bytes(prf, data, &auth);
   ```

2. **我们的错误**: 之前的修复直接拒绝NULL参数：
   ```c
   if (!bytes)
       return FALSE;  // 🚨 错误！拒绝了增量模式
   ```

3. **症状表现**: `collect_int_auth_data returned FAILED`

---

## ✅ 最终修复清单

### 修复 1: PRF-SM3 增量模式支持

**文件**: `plugins/gmalg/gmalg_hasher.c`

添加 `pending` 缓冲区支持增量模式：

```c
struct private_gmalg_sm3_prf_t {
    gmalg_sm3_prf_t public;
    chunk_t key;
    chunk_t pending;  // <- 新增：增量模式缓存
};

METHOD(prf_t, get_bytes, bool, ...)
{
    /* 【Incremental Mode】: if bytes is NULL, just cache the data */
    if (!bytes)
    {
        chunk_t new_pending = chunk_cat("cc", this->pending, seed);
        chunk_free(&this->pending);
        this->pending = new_pending;
        return TRUE;
    }

    /* 【Output Mode】: combine pending cache with current seed */
    full_seed = chunk_cat("cc", this->pending, seed);
    // ... compute HMAC-SM3 ...
    // ... reset pending after output ...
}
```

### 修复 2: HMAC-SM3 Key Length 定义

**文件**: `sa/keymat.c`

```c
keylen_entry_t map[] = {
    // ... existing entries ...
    {AUTH_HMAC_SM3_128,  128},  // <- 新增
    {AUTH_HMAC_SM3_256,  256},  // <- 新增
};
```

### 修复 3: 枚举名称注册

已添加以下GM算法的枚举名称：

| 文件 | 枚举 | 值 |
|------|------|----|
| `crypto/hashers/hasher.c` | HASH_SM3 | 1032 |
| `crypto/prfs/prf.c` | PRF_SM3 | 1052 |
| `crypto/crypters/crypter.c` | ENCR_SM4_* | 1040-1042 |
| `crypto/signers/signer.c` | AUTH_HMAC_SM3_* | 1056-1057 |
| `crypto/key_exchange.c` | KE_SM2 | 1051 |

---

## ⚠️ 已知限制

### ESP层SM4-CBC内核不支持

**问题**: Linux内核不支持 `cbc(sm4)` 作为ESP加密算法

**表现**:
```
[KNL] algorithm SM4_CBC not supported by kernel!
[IKE] unable to install inbound and outbound IPsec SA (SAD) in kernel
```

**解决方案**:
1. **临时方案**: ESP使用AES-GCM（内核支持），IKE层使用国密对称栈
2. **长期方案**: 启用strongSwan的`kernel-libipsec`用户态ESP实现

---

## 关键结论

### ✅ 正确的分析

1. **PRF-SM3 缺少增量模式**是 `collect_int_auth_data` 失败的根因
2. **HMAC-SM3 key length 未定义**是 CHILD_SA 建立失败的原因
3. **内核不支持SM4-CBC**是ESP安装失败的硬件限制

### ❌ 错误的分析（已被纠正）

- ~~`skp_build` 为空导致 `prf->set_key` 失败~~
- ~~PRF-SM3 内存越界~~（这是之前版本的问题，已修复）
- ~~分片机制问题~~

---

## 相关文件

| 文件 | 作用 | 状态 |
|------|------|------|
| `gmalg_hasher.c` | SM3 Hash + PRF 实现 | ✅ 已修复（增量模式） |
| `gmalg_hmac_signer.c` | HMAC-SM3 实现 | ✅ 已修复（增量模式） |
| `sa/keymat.c` | Key length 定义 | ✅ 已添加SM3支持 |
| `crypto/*/signer.c` | 枚举名称注册 | ✅ 已添加 |

---

## 版本信息

- strongSwan: 6.0.4 (修改版)
- GmSSL: 3.1.1
- 操作系统: Ubuntu 22.04 (Docker 容器)
- 内核: Linux 6.8.0-101-generic

---

## 更新历史

| 日期 | 更新内容 |
|------|----------|
| 2026-03-01 | 初始文档，记录 HMAC-SM3 增量模式修复 |
| 2026-03-04 | 根据外部 AI 分析纠正根本原因，记录 PRF-SM3 内存越界修复 |
| 2026-03-04 19:30 | **✅ 完全解决** - PRF-SM3增量模式 + HMAC-SM3 key length |
