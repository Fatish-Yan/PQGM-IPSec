# 国密算法集成实现总结

> **文档版本**: 2026-03-04
> **状态**: IKE层国密对称栈已完全实现并测试通过

---

## 概述

本文档总结了在 strongSwan 6.0.4 中集成国密算法（SM2/SM3/SM4）的实现细节，包括：
- IKE层的国密对称栈（SM4 + HMAC-SM3 + PRF-SM3）
- SM2-KEM 密钥交换
- 双证书机制（签名证书 + 加密证书）

---

## 1. 国密算法ID分配

使用 IANA 私有使用范围 (1024-65535)：

| 算法 | ID | 说明 |
|------|-----|------|
| HASH_SM3 | 1032 | SM3 哈希算法 |
| ENCR_SM4_ECB | 1040 | SM4 ECB 模式 |
| ENCR_SM4_CBC | 1041 | SM4 CBC 模式 |
| ENCR_SM4_CTR | 1042 | SM4 CTR 模式 |
| KE_SM2 | 1051 | SM2-KEM 密钥交换 |
| PRF_SM3 | 1052 | HMAC-SM3 PRF |
| AUTH_HMAC_SM3_128 | 1056 | HMAC-SM3 128位截断 |
| AUTH_HMAC_SM3_256 | 1057 | HMAC-SM3 256位完整 |

---

## 2. gmalg 插件架构

```
strongswan/src/libstrongswan/plugins/gmalg/
├── gmalg_plugin.c/h      # 插件入口，算法注册
├── gmalg_hasher.c/h      # SM3 Hash + PRF 实现
├── gmalg_crypter.c/h     # SM4 ECB/CBC/CTR 实现
├── gmalg_signer.c/h      # SM2 签名
├── gmalg_kem.c/h         # SM2-KEM 密钥封装
└── gmalg_hmac_signer.c/h # HMAC-SM3 AEAD 签名器
```

### 2.1 SM3 Hash 实现

```c
// gmalg_hasher.c
typedef struct private_gmalg_sm3_hasher_t {
    gmalg_sm3_hasher_t public;
    SM3_CTX ctx;  // GmSSL SM3 上下文
} private_gmalg_sm3_hasher_t;

// 核心方法
sm3_init(&ctx);
sm3_update(&ctx, data, len);
sm3_finish(&ctx, digest);
```

### 2.2 PRF-SM3 实现（关键修复）

**问题**: RFC 9242 IntAuth 计算需要 PRF 支持增量模式

**解决方案**: 添加 `pending` 缓冲区

```c
struct private_gmalg_sm3_prf_t {
    gmalg_sm3_prf_t public;
    chunk_t key;
    chunk_t pending;  // 增量模式缓存
};

METHOD(prf_t, get_bytes, bool, ...)
{
    if (!bytes) {
        // 增量模式：缓存数据
        chunk_t new_pending = chunk_cat("cc", this->pending, seed);
        chunk_free(&this->pending);
        this->pending = new_pending;
        return TRUE;
    }

    // 输出模式：合并缓存计算 HMAC-SM3
    full_seed = chunk_cat("cc", this->pending, seed);
    // HMAC-SM3 = SM3((K⊕opad) || SM3((K⊕ipad) || full_seed))
    ...
}
```

### 2.3 HMAC-SM3 AEAD 签名器

**问题**: strongSwan 的 AEAD wrapper 需要增量签名模式

**解决方案**: 在 `gmalg_hmac_signer.c` 中添加 pending buffer

```c
struct private_hmac_sm3_signer_t {
    hmac_sm3_signer_t public;
    uint8_t *pending;
    size_t pending_len;
    size_t pending_cap;
};

METHOD(signer_t, get_signature, bool, ...)
{
    if (!buffer) {
        // 增量模式：累积数据
        ensure_pending_capacity(this, this->pending_len + data.len);
        memcpy(this->pending + this->pending_len, data.ptr, data.len);
        this->pending_len += data.len;
        return TRUE;
    }
    // 最终模式：计算 HMAC
    compute_hmac_sm3_final(this, data, buffer);
    reset_pending(this);
    return TRUE;
}
```

---

## 3. 枚举名称注册

需要在以下文件中添加国密算法的枚举名称：

### 3.1 crypto/hashers/hasher.c

```c
ENUM_NEXT(hash_algorithm_names, HASH_SM3, HASH_SM3, HASH_SHA3_512,
    "HASH_SM3");
ENUM_END(hash_algorithm_names, HASH_SM3);

ENUM_NEXT(hash_algorithm_short_names, HASH_SM3, HASH_SM3, HASH_SHA3_512,
    "sm3");
ENUM_END(hash_algorithm_short_names, HASH_SM3);
```

### 3.2 crypto/prfs/prf.c

```c
ENUM_NEXT(pseudo_random_function_names, PRF_SM3, PRF_SM3, PRF_AES128_CMAC,
    "PRF_SM3");
ENUM_END(pseudo_random_function_names, PRF_SM3);
```

### 3.3 crypto/signers/signer.c

```c
ENUM_NEXT(integrity_algorithm_names, AUTH_HMAC_SM3_128, AUTH_HMAC_SM3_256,
    AUTH_HMAC_SHA2_512_256,
    "HMAC_SM3_128",
    "HMAC_SM3_256");
ENUM_END(integrity_algorithm_names, AUTH_HMAC_SM3_256);
```

### 3.4 crypto/crypters/crypter.c

```c
ENUM_NEXT(encryption_algorithm_names, ENCR_SM4_ECB, ENCR_SM4_CTR, ENCR_AES_CFB,
    "SM4_ECB",
    "SM4_CBC",
    "SM4_CTR");
ENUM_END(encryption_algorithm_names, ENCR_SM4_CTR);
```

### 3.5 sa/keymat.c - 密钥长度定义

```c
keylen_entry_t map[] = {
    // ... existing entries ...
    {AUTH_HMAC_SM3_128,  128},
    {AUTH_HMAC_SM3_256,  256},
};
```

---

## 4. SM2-KEM 实现

### 4.1 密钥封装机制

```c
// gmalg_kem.c
METHOD(key_exchange_t, get_public_key, bool, ...)
{
    // 1. 生成临时 SM2 密钥对
    // 2. 使用对端公钥加密 peer_random
    // 3. 返回密文 (139 bytes = 1 + 32 + 32 + 32 + 4 + 38)
    //    - ASN.1 头: 1 byte
    //    - C1 (椭圆曲线点): 65 bytes (压缩后 33 bytes)
    //    - C2 (加密的 peer_random): 32 bytes
    //    - C3 (SM3 哈希): 32 bytes
    //    - ASN.1 结构开销: ~10 bytes
}

METHOD(key_exchange_t, set_public_key, bool, ...)
{
    // 1. 解析密文
    // 2. 使用本端私钥解密得到对端 peer_random
    // 3. 合并 peer_random (32 bytes) + my_random (32 bytes) = 64 bytes shared secret
}
```

### 4.2 双证书机制

- **SignCert**: SM2 签名证书（用于身份认证）
- **EncCert**: SM2 加密证书（用于 SM2-KEM）

在 IKE_INTERMEDIATE #0 中交换两个证书：
```
[ENC] generating IKE_INTERMEDIATE request 1 [ CERT CERT ]
```

---

## 5. 国密对称栈配置

### 5.1 IKE 提案

```
proposals = sm4-hmacsm3-prfsm3-x25519-ke1_sm2kem-ke2_mlkem768
```

解析为：
- 加密: SM4-CBC-128
- 完整性: HMAC-SM3-128
- PRF: PRF-SM3 (HMAC-SM3)
- DH: X25519
- KE1: SM2-KEM
- KE2: ML-KEM-768

### 5.2 ESP 提案

```
esp_proposals = sm4-hmacsm3
```

**注意**: Linux 内核不支持 `cbc(sm4)`，ESP 层需要使用 AES-GCM 或启用 `kernel-libipsec`。

---

## 6. 测试结果

### 6.1 5-RTT 握手流程

```
RTT 1: IKE_SA_INIT
  ✅ 提案协商: SM4_CBC_128/HMAC_SM3_128/PRF_SM3
  ✅ 密钥派生: SKEYSEED, SK_d, SK_pi, SK_pr

RTT 2: IKE_INTERMEDIATE #0
  ✅ 双证书交换: SignCert + EncCert

RTT 3: IKE_INTERMEDIATE #1
  ✅ SM2-KEM 密钥交换
  ✅ 密钥更新: SK_d, SK_pi, SK_pr

RTT 4: IKE_INTERMEDIATE #2
  ✅ ML-KEM-768 密钥交换
  ✅ 密钥更新: SK_d, SK_pi, SK_pr

RTT 5: IKE_AUTH
  ✅ ML-DSA-65 签名认证
  ✅ CHILD_SA 建立
```

### 6.2 关键日志

```
[CFG] selected proposal: IKE:SM4_CBC_128/HMAC_SM3_128/PRF_SM3/CURVE_25519/KE1_KE_SM2/KE2_ML_KEM_768
[IKE] RFC 9370 Key Derivation: Initial (IKE_SA_INIT)
[IKE]   SKEYSEED derived from Ni|Nr and DH shared secret
[IKE]   SK_d/SK_pi/SK_pr 成功派生
[LIB] AEAD encrypt: SUCCESS
[IKE] SM2-KEM: computed shared secret (64 bytes)
[IKE] RFC 9370 Key Derivation: Update after IKE_INTERMEDIATE KE
[IKE] authentication of 'responder.pqgm.test' with (23) successful
[IKE] IKE_SA pqgm-5rtt-gm-symm[1] established!
[IKE] CHILD_SA net{1} established!
```

---

## 7. 已知限制与解决方案

### 7.1 ESP 层 SM4 内核不支持

**问题**: Linux 内核 crypto API 不支持 `cbc(sm4)`

**解决方案**:
1. **临时**: ESP 使用 AES-GCM，IKE 使用国密对称栈
2. **长期**: 启用 strongSwan 的 `--enable-kernel-libipsec` 用户态 ESP

### 7.2 性能考虑

- SM3 性能: ~443 MB/s (vs SHA-256 ~800 MB/s)
- SM4 性能: ~175 MB/s (vs AES-CBC ~500 MB/s)
- SM2-KEM: 需要预加载私钥优化性能

---

## 8. 参考资料

- GM/T 0002-2012: SM2 椭圆曲线公钥密码算法
- GM/T 0003-2012: SM3 密码杂凑算法
- GM/T 0004-2012: SM4 分组密码算法
- GM/T 0022: IPsec VPN 技术规范
- RFC 9242: Intermediate Exchange in the IKEv2 Protocol
- RFC 9370: Multiple Key Exchanges in the IKEv2 Protocol
- GmSSL 3.1.3 API 文档

---

## 9. 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `plugins/gmalg/gmalg_hasher.c` | SM3 Hash + PRF (增量模式) |
| `plugins/gmalg/gmalg_crypter.c` | SM4 ECB/CBC/CTR |
| `plugins/gmalg/gmalg_signer.c` | SM2 签名 |
| `plugins/gmalg/gmalg_kem.c` | SM2-KEM |
| `plugins/gmalg/gmalg_hmac_signer.c` | HMAC-SM3 AEAD (增量模式) |
| `crypto/hashers/hasher.c` | HASH_SM3 枚举 |
| `crypto/prfs/prf.c` | PRF_SM3 枚举 |
| `crypto/signers/signer.c` | AUTH_HMAC_SM3 枚举 |
| `crypto/crypters/crypter.c` | ENCR_SM4 枚举 |
| `crypto/key_exchange.c` | KE_SM2 枚举 |
| `sa/keymat.c` | HMAC-SM3 key length |
