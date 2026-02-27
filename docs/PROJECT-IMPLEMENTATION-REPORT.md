# PQ-GM-IKEv2 项目实现报告

> 生成时间: 2026-02-27
> 目的: 作为论文"系统实现与验证"章节的参考材料

---

## 1. 项目概述

### 1.1 项目目标

PQ-GM-IKEv2 是一个抗量子与国密融合的 IKEv2/IPSec 协议实现，核心创新包括：

1. **混合密钥交换**: 经典 DH + 后量子 KEM (ML-KEM) + 国密 SM2-KEM
2. **双证书机制**: SM2 签名证书/加密证书分离
3. **基于 RFC 9242/9370**: 利用 IKE_INTERMEDIATE 和多重密钥交换框架

### 1.2 开发环境

| 组件 | 版本 | 说明 |
|------|------|------|
| 操作系统 | Ubuntu 22.04 / Linux 6.8.0 | VMware 虚拟机 |
| strongSwan | 6.0.4 | VPN 框架 |
| GmSSL | 3.1.3 | 国密算法库 |
| 编译器 | GCC 11.x | C 语言开发 |

### 1.3 Git 提交历史概览

项目共 **26 次提交**，开发周期约 2 天（2026-02-26 ~ 2026-02-27）：

```
e860cbe - 初始提交: PQGM-IKEv2 项目框架
ca07c1f - 实现 SM3 hasher 和 SM4 crypter
a1a6da5 - 修复 gmalg 插件编译和加载问题
e4135be - 修复 SM2 签名验证
4230d15 - 添加 SM4 CTR 模式
c77876d - 添加 SM2 双证书生成
f928c97 - SM2-KEM 实现完成
3104a2f - M5 协议集成实现
778d380 - ML-KEM 混合密钥交换验证
```

---

## 2. 模块化实现

### 2.1 模块划分

```
┌─────────────────────────────────────────────────────────────────┐
│                        PQ-GM-IKEv2 系统                          │
├─────────────────────────────────────────────────────────────────┤
│  Module 1     │  Module 2     │  Module 3     │  Module 4       │
│  基础算法     │  SM2-KEM      │  证书机制     │  ML-KEM         │
│  (已完成)     │  (已完成)     │  (已完成)     │  (已完成)       │
├─────────────────────────────────────────────────────────────────┤
│  Module 5           │  Module 6                                 │
│  协议集成           │  测试评估                                 │
│  (已完成)           │  (进行中)                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Module 1: 基础国密算法实现

### 3.1 概述

在 strongSwan 中实现 GmSSL 国密算法的插件封装，提供 SM3 哈希、SM4 对称加密、SM2 签名功能。

### 3.2 插件架构

**目录结构**:
```
strongswan/src/libstrongswan/plugins/gmalg/
├── gmalg_plugin.c/h      # 插件入口，算法注册
├── gmalg_hasher.c/h      # SM3 哈希 + PRF
├── gmalg_crypter.c/h     # SM4 ECB/CBC/CTR
├── gmalg_signer.c/h      # SM2 签名
└── gmalg_ke.c/h          # SM2-KEM (Module 2)
```

### 3.3 算法 ID 定义 (私有使用范围)

```c
// gmalg_plugin.h
#define HASH_SM3        1032    // SM3 Hash
#define ENCR_SM4_ECB    1040    // SM4 ECB mode
#define ENCR_SM4_CBC    1041    // SM4 CBC mode
#define ENCR_SM4_CTR    1042    // SM4 CTR mode
#define AUTH_SM2        1050    // SM2 Signature
#define KE_SM2          1051    // SM2-KEM
#define PRF_SM3         1052    // SM3 PRF
```

### 3.4 SM3 哈希实现

**文件**: `gmalg_hasher.c`

**核心结构**:
```c
typedef struct private_gmalg_sm3_hasher_t {
    gmalg_sm3_hasher_t public;
    SM3_CTX ctx;  // GmSSL SM3 上下文
} private_gmalg_sm3_hasher_t;
```

**关键实现**:
```c
// 使用 GmSSL 3.1.3 API
static void sm3_reset(private_gmalg_sm3_hasher_t *this) {
    sm3_init(&this->ctx);
}

static void sm3_update_ctx(private_gmalg_sm3_hasher_t *this,
                           const uint8_t *data, size_t len) {
    sm3_update(&this->ctx, data, len);
}

static void sm3_final(private_gmalg_sm3_hasher_t *this, uint8_t *digest) {
    sm3_finish(&this->ctx, digest);
}
```

**性能数据**:
- 吞吐量: **443 MB/s**
- 哈希操作: **7,094 次/秒**

### 3.5 SM4 对称加密实现

**文件**: `gmalg_crypter.c`

**支持模式**: ECB / CBC / CTR

**核心结构**:
```c
typedef struct private_gmalg_sm4_crypter_t {
    gmalg_sm4_crypter_t public;
    SM4_KEY enc_key;     // 加密密钥
    SM4_KEY dec_key;     // 解密密钥
    sm4_mode_t mode;     // 加密模式
    size_t key_size;     // 密钥大小 (16 字节)
} private_gmalg_sm4_crypter_t;
```

**关键实现**:
```c
METHOD(crypter_t, encrypt, bool, ...) {
    switch (this->mode) {
        case SM4_MODE_ECB:
            sm4_encrypt_blocks(&this->enc_key, in, nblocks, out);
            break;
        case SM4_MODE_CBC:
            sm4_cbc_encrypt_blocks(&this->enc_key, iv_copy, in, nblocks, out);
            break;
        case SM4_MODE_CTR:
            sm4_ctr_encrypt_blocks(&this->enc_key, ctr, in, nblocks, out);
            break;
    }
    return TRUE;
}
```

**性能数据**:
| 模式 | 加密吞吐量 | 解密吞吐量 |
|------|-----------|-----------|
| ECB | 189 MB/s | 190 MB/s |
| CBC | 161 MB/s | 175 MB/s |

### 3.6 SM2 签名实现

**文件**: `gmalg_signer.c`

**核心结构**:
```c
typedef struct private_gmalg_sm2_signer_t {
    gmalg_sm2_signer_t public;
    SM2_KEY sm2_key;           // GmSSL SM2 密钥上下文
    bool has_private_key;      // 是否有私钥
    size_t key_size;           // 密钥大小
} private_gmalg_sm2_signer_t;
```

**签名流程**:
```c
// 1. SM3 哈希
sm3_init(&ctx);
sm3_update(&ctx, data.ptr, data.len);
sm3_finish(&ctx, dgst);

// 2. SM2 签名 (DER 编码)
sm2_sign(&this->sm2_key, dgst, signature->ptr, &signature->len);
```

**验证流程**:
```c
// 1. SM3 哈希
sm3_finish(&ctx, dgst);

// 2. 处理 DER 编码长度
actual_len = 2 + signature.ptr[1];

// 3. SM2 验签
sm2_verify(&this->sm2_key, dgst, signature.ptr, actual_len);
```

**支持的密钥格式**:
1. DER 编码私钥信息 (`sm2_private_key_info_from_der`)
2. DER 编码公钥信息 (`sm2_public_key_info_from_der`)
3. 原始私钥 (32 字节)

### 3.7 插件注册

**文件**: `gmalg_plugin.c`

```c
METHOD(plugin_t, get_features, int, ...) {
    static plugin_feature_t f[] = {
#ifdef HAVE_GMSSL
        // SM3 Hash
        PLUGIN_REGISTER(HASHER, gmalg_sm3_hasher_create),
            PLUGIN_PROVIDE(HASHER, HASH_SM3),

        // SM3 PRF
        PLUGIN_REGISTER(PRF, gmalg_sm3_prf_create),
            PLUGIN_PROVIDE(PRF, PRF_SM3),

        // SM4 ECB/CBC/CTR
        PLUGIN_REGISTER(CRYPTER, gmalg_sm4_crypter_create),
            PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_ECB, 16),
        PLUGIN_REGISTER(CRYPTER, gmalg_sm4_cbc_crypter_create),
            PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_CBC, 16),
        PLUGIN_REGISTER(CRYPTER, gmalg_sm4_ctr_crypter_create),
            PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_CTR, 16),

        // SM2 Signature
        PLUGIN_REGISTER(SIGNER, gmalg_sm2_signer_create),
            PLUGIN_PROVIDE(SIGNER, AUTH_SM2),

        // SM2-KEM
        PLUGIN_REGISTER(KE, gmalg_sm2_ke_create),
            PLUGIN_PROVIDE(KE, KE_SM2),
#endif
    };
    *features = f;
    return countof(f);
}
```

### 3.8 编译配置

**Makefile.am**:
```makefile
AM_CPPFLAGS = -I$(top_srcdir)/src/libstrongswan

if MONOLITHIC
libstrongswan_gmalg_la_LIBADD = -L/usr/local/lib -lgmssl
else
libstrongswan_gmalg_la_LIBADD = $(top_builddir)/src/libstrongswan/libstrongswan.la -lgmssl
endif
```

**configure.ac 添加**:
```
AC_ARG_ENABLE([gmalg],
    AS_HELP_STRING([--enable-gmalg], [enable GmSSL algorithm plugin]))
if test x$gmalg = xtrue; then
    AC_DEFINE([HAVE_GMSSL], [1], [Define if GmSSL is available])
fi
```

---

## 4. Module 2: SM2-KEM 密钥交换

### 4.1 概述

实现双向 SM2-KEM 密钥封装机制，用于 IKE_INTERMEDIATE 阶段的密钥交换。

### 4.2 协议设计

```
Initiator                           Responder
    |                                   |
    |--- 1. 发送 EncCert 公钥 --------->|
    |                                   |
    |<-- 2. 返回 EncCert 公钥 ----------|
    |                                   |
    |--- 3. 发送 SM2_Enc(r_i) --------->|
    |<-- 4. 返回 SM2_Enc(r_r) ----------|
    |                                   |
    |     共享密钥 SK = r_i || r_r      |
```

### 4.3 核心实现

**文件**: `gmalg_ke.c`

**常量定义**:
```c
#define SM2_KEM_PUBLIC_KEY_SIZE     65   // 非压缩公钥: 0x04 || X || Y
#define SM2_KEM_RANDOM_SIZE         32   // 随机贡献大小
#define SM2_KEM_SHARED_SECRET_SIZE  64   // SK = r_i || r_r
#define SM2_KEM_CIPHERTEXT_SIZE     366  // DER 编码密文最大值
```

**核心结构**:
```c
struct private_key_exchange_t {
    key_exchange_t public;
    key_exchange_method_t method;     // KE_SM2 = 1051

    SM2_KEY my_key;                   // 本方密钥对
    chunk_t my_pubkey;                // 本方公钥导出

    SM2_KEY peer_enccert;             // 对方 EncCert 公钥
    bool has_peer_enccert;

    chunk_t my_random;                // 本方随机 (r_i 或 r_r)
    chunk_t peer_random;              // 对方随机
    chunk_t shared_secret;            // 最终共享密钥
    chunk_t my_ciphertext;            // 封装密文

    bool is_initiator;                // 角色标识
};
```

**封装流程**:
```c
// 1. 生成随机贡献
this->my_random = chunk_alloc(SM2_KEM_RANDOM_SIZE);
rand_bytes(this->my_random.ptr, SM2_KEM_RANDOM_SIZE);

// 2. 使用对方 EncCert 公钥加密
sm2_encrypt(&this->peer_enccert,
            this->my_random.ptr, this->my_random.len,
            ciphertext, &ctlen);
```

**解封装流程**:
```c
// 1. 使用本方私钥解密
sm2_decrypt(&this->my_key,
            ciphertext.ptr, ciphertext.len,
            plaintext, &ptlen);

// 2. 存储对方随机
this->peer_random = chunk_alloc(SM2_KEM_RANDOM_SIZE);
memcpy(this->peer_random.ptr, plaintext, ptlen);
```

**共享密钥计算**:
```c
static bool compute_shared_secret(private_key_exchange_t *this) {
    this->shared_secret = chunk_alloc(SM2_KEM_SHARED_SECRET_SIZE);

    if (this->is_initiator) {
        // Initiator: SK = r_i || r_r
        memcpy(this->shared_secret.ptr, this->my_random.ptr, 32);
        memcpy(this->shared_secret.ptr + 32, this->peer_random.ptr, 32);
    } else {
        // Responder: SK = r_i || r_r
        memcpy(this->shared_secret.ptr, this->peer_random.ptr, 32);
        memcpy(this->shared_secret.ptr + 32, this->my_random.ptr, 32);
    }
    return TRUE;
}
```

### 4.4 测试结果

- 共享密钥大小: **64 字节** (r_i || r_r)
- 密文大小: **141 字节** (DER 编码)
- 所有功能测试通过

---

## 5. Module 3: 双证书机制

### 5.1 概述

实现 SM2 双证书机制，分离签名证书 (SignCert) 和加密证书 (EncCert)。

### 5.2 证书类型

| 证书类型 | Key Usage | Extended Key Usage | 用途 |
|---------|-----------|-------------------|------|
| SignCert | digitalSignature, nonRepudiation | - | IKE_AUTH 签名 |
| EncCert | keyEncipherment | ikeIntermediate | SM2-KEM 加密 |
| AuthCert | - | - | 后量子认证 (SPHINCS+) |

### 5.3 证书生成

**脚本**: `scripts/gen_certs.sh`

**生成流程**:
```bash
# 1. 生成 CA 证书
gmssl sm2keygen -out ca_key.pem
gmssl req -new -x509 -key ca_key.pem -out ca_sm2_cert.pem

# 2. 生成签名证书
gmssl sm2keygen -out sign_key.pem
gmssl req -new -key sign_key.pem | gmssl x509 -req -CA ca_sm2_cert.pem \
    -extfile sign_ext.cnf -out sign_cert.pem

# 3. 生成加密证书
gmssl sm2keygen -out enc_key.pem
gmssl req -new -key enc_key.pem | gmssl x509 -req -CA ca_sm2_cert.pem \
    -extfile enc_ext.cnf -out enc_cert.pem
```

### 5.4 IKE_INTERMEDIATE #0 证书分发

**修改文件**: `ike_cert_post.c`

**核心逻辑**:
```c
// 检查是否是第一个 IKE_INTERMEDIATE 消息
if (message->get_message_id(message) == 1) {
    // 发送 SignCert
    add_cert(cert_payload, sign_cert);

    // 发送 EncCert (通过 EKU ikeIntermediate 识别)
    if (has_ike_intermediate_eku(enc_cert)) {
        add_cert(cert_payload, enc_cert);
    }
}
```

**消息 ID 判断原理**:
- IKE_SA_INIT: 消息 ID = 0
- IKE_INTERMEDIATE #0: 消息 ID = 1
- IKE_INTERMEDIATE #1: 消息 ID = 2
- ...

---

## 6. Module 4: ML-KEM 集成

### 6.1 概述

集成 strongSwan 内置的 ml 插件，支持 ML-KEM-768 后量子密钥交换。

### 6.2 配置

**swanctl.conf**:
```conf
connections {
    pqgm-mlkem {
        version = 2
        proposals = aes256-sha384-x25519-ke1_mlkem768

        children {
            ipsec {
                esp_proposals = aes256gcm256-x25519-ke1_mlkem768
            }
        }
    }
}
```

### 6.3 测试结果

| 配置 | 密钥交换方法 | RTT | 平均时延 | 成功率 |
|------|-------------|-----|----------|--------|
| 传统 IKEv2 | x25519 | 2 | 48 ms | 100% |
| 混合密钥交换 | x25519 + ML-KEM-768 | 3 | 52 ms | 100% |

**结论**: 混合密钥交换增加约 **4ms** (8.3%) 时延，ML-KEM-768 密文约 **1184 字节**

---

## 7. Module 5: 协议集成

### 7.1 概述

将 M1-M4 模块集成到完整的 PQ-GM-IKEv2 协议流程。

### 7.2 协议流程

```
RTT 1: IKE_SA_INIT
       协商 KE=x25519, ADDKE1=ml-kem-768, ADDKE2=sm2-kem
       │
RTT 2: IKE_INTERMEDIATE #0 (消息 ID = 1)
       交换双证书: SignCert + EncCert
       │
RTT 3: IKE_INTERMEDIATE #1 (消息 ID = 2)
       SM2-KEM 密钥交换 (141 字节密文)
       │
RTT 4: IKE_INTERMEDIATE #2 (消息 ID = 3)
       ML-KEM-768 密钥交换 (1184 字节密文)
       │
RTT 5: IKE_AUTH (消息 ID = 4)
       SM2 签名认证 (后量子认证待扩展)
```

### 7.3 密钥派生 (RFC 9370)

```c
// 1. INITIAL_KEY_MAT (IKE_SA_INIT)
keymat_0 = prf(skeyseed, Ni | Nr | SPIi | SPIr)

// 2. additional_key_mat_1 (ADDKE1: ML-KEM-768)
keymat_1 = prf(keymat_0, "additional key material 1" | DH1)

// 3. additional_key_mat_2 (ADDKE2: SM2-KEM)
keymat_2 = prf(keymat_1, "additional key material 2" | DH2)

// 4. 最终 KEYMAT
final_keymat = keymat_2
```

### 7.4 配置文件

**三重密钥交换配置**:
```conf
connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.1.1
        remote_addrs = 192.168.1.2

        # 三重密钥交换提案
        proposals = aes256-sha384-x25519-ke1_mlkem768-ke2_sm2kem

        local {
            auth = pubkey
            certs = sign_cert.pem
            id = initiator.pqgm.test
        }

        remote {
            auth = pubkey
            id = responder.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.1.0.0/16
                remote_ts = 10.2.0.0/16
                esp_proposals = aes256gcm256-x25519-ke1_mlkem768-ke2_sm2kem
            }
        }
    }
}
```

### 7.5 代码修改清单

| 文件 | 修改内容 | 状态 |
|------|----------|------|
| `gmalg_plugin.c` | 注册 SM3/SM4/SM2/SM2-KEM | ✅ |
| `gmalg_hasher.c` | SM3 Hash + PRF 实现 | ✅ |
| `gmalg_crypter.c` | SM4 ECB/CBC/CTR 实现 | ✅ |
| `gmalg_signer.c` | SM2 签名实现 | ✅ |
| `gmalg_ke.c` | SM2-KEM 双向封装 | ✅ |
| `ike_cert_post.c` | IKE_INTERMEDIATE #0 证书分发 | ✅ |
| `configure.ac` | gmalg 插件编译配置 | ✅ |
| `Makefile.am` | gmalg 链接 GmSSL | ✅ |

---

## 8. 测试与验证

### 8.1 功能测试

**测试程序**: `tests/test_gmalg.c`

**测试用例**:
| 测试项 | 结果 |
|--------|------|
| SM3 哈希功能 | ✅ 通过 |
| SM3 PRF 功能 | ✅ 通过 |
| SM4 ECB 加解密 | ✅ 通过 |
| SM4 CBC 加解密 | ✅ 通过 |
| SM4 CTR 加解密 | ✅ 通过 |
| SM2 签名验证 | ✅ 通过 |
| SM2-KEM 密钥交换 | ✅ 通过 |

### 8.2 性能测试

**测试程序**: `tests/benchmark_gmalg.c`

**SM3 性能**:
- 吞吐量: **443 MB/s**
- 哈希操作: **7,094 次/秒**

**SM4 性能**:
| 模式 | 加密吞吐量 | 解密吞吐量 |
|------|-----------|-----------|
| ECB | 189 MB/s | 190 MB/s |
| CBC | 161 MB/s | 175 MB/s |

**SM3 PRF 性能**:
- 操作速率: **3,701,173 次/秒**

### 8.3 ML-KEM 混合测试

| 配置 | RTT | 时延 | 密文大小 |
|------|-----|------|---------|
| x25519 | 2 | 48 ms | 32 B |
| x25519 + ML-KEM-768 | 3 | 52 ms | 32 B + 1184 B |

---

## 9. GmSSL API 使用要点

### 9.1 GmSSL 3.1.3 vs 旧版本 API 差异

**SM3 哈希**:
```c
// ❌ 旧版本 (不存在)
sm3_hash(data, len, digest);

// ✅ GmSSL 3.1.3
SM3_CTX ctx;
sm3_init(&ctx);
sm3_update(&ctx, data, len);
sm3_finish(&ctx, digest);
```

**SM2 密钥初始化**:
```c
// ❌ 旧版本 (不存在)
sm2_key_init(&key);

// ✅ GmSSL 3.1.3
memset(&key, 0, sizeof(SM2_KEY));
```

**SM2 私钥设置**:
```c
// 注意参数类型是 const uint64_t*
sm2_key_set_private_key(&key, (const uint64_t*)private_key_bytes);
```

### 9.2 常见问题解决

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| `HAVE_GMSSL` 未定义 | configure 未检测到 GmSSL | 手动在 config.h 添加 |
| 编译链接失败 | GmSSL 库路径未指定 | 添加 `-L/usr/local/lib -lgmssl` |
| 方法未使用警告 | 条件编译导致 | 使用 `#ifdef HAVE_GMSSL` 包裹 |

---

## 10. 项目目录结构

```
PQGM-IPSec/
├── certs/                      # 证书文件
│   ├── ca/                     # CA 证书
│   ├── initiator/              # 发起方证书
│   └── responder/              # 响应方证书
├── configs/                    # 配置文件
│   ├── swanctl-simple.conf     # 简单配置
│   ├── swanctl-mlkem-test.conf # ML-KEM 测试配置
│   └── swanctl-loopback.conf   # 本地回环测试配置
├── docs/                       # 文档
│   └── plans/                  # 实现计划
├── scripts/                    # 脚本
│   ├── gen_certs.sh            # 证书生成
│   └── gen_certs_pki.sh        # PKI 版本
├── strongswan/                 # strongSwan 修改
│   └── gmalg/                  # gmalg 插件源码
├── tests/                      # 测试程序
│   ├── test_gmalg.c            # 功能测试
│   ├── benchmark_gmalg.c       # 性能测试
│   └── test_m3_cert_dist.c     # M3 模块测试
├── CLAUDE.md                   # 项目指南
├── MODULES.md                  # 模块状态
├── PROJECT.md                  # 项目文档
└── test_results.md             # 测试结果
```

---

## 11. 总结

### 11.1 完成情况

| 模块 | 状态 | 完成度 |
|------|------|--------|
| M1: 基础算法 (SM3/SM4/SM2-Sign) | ✅ | 100% |
| M2: SM2-KEM | ✅ | 100% |
| M3: 双证书机制 | ✅ | 100% |
| M4: ML-KEM 集成 | ✅ | 100% |
| M5: 协议集成 | ✅ | 90% (待双机测试) |
| M6: 测试评估 | 🔄 | 50% |

### 11.2 关键成果

1. **gmalg 插件**: 完整的国密算法插件实现
2. **SM2-KEM**: 64 字节共享密钥，141 字节密文
3. **双证书分发**: IKE_INTERMEDIATE #0 机制
4. **三重密钥交换**: x25519 + ML-KEM-768 + SM2-KEM
5. **5 RTT 协议**: 完整的 PQ-GM-IKEv2 流程

### 11.3 待完成工作

1. 双机端到端测试
2. IKE_AUTH 后量子签名认证 (ML-DSA/SLH-DSA)
3. 性能基准测试数据收集
4. 论文数据整理

---

## 12. 参考资料

- RFC 7296: Internet Key Exchange Protocol Version 2 (IKEv2)
- RFC 9242: Intermediate Exchange in the Internet Key Exchange Protocol Version 2 (IKEv2)
- RFC 9370: Multiple Key Exchanges in the Internet Key Exchange Protocol Version 2 (IKEv2)
- FIPS 203: Module-Lattice-Based Key-Encapsulation Mechanism Standard (ML-KEM)
- FIPS 204: Module-Lattice-Based Digital Signature Standard (ML-DSA)
- GM/T 0002-2012: SM4 Block Cipher Algorithm
- GM/T 0003-2012: SM2 Elliptic Curve Public Key Cryptography
- GM/T 0004-2012: SM3 Cryptographic Hash Algorithm
- strongSwan 6.0 Documentation: https://docs.strongswan.org/
- GmSSL 3.1.3 Documentation: https://github.com/guanzhi/GmSSL

---

*报告生成时间: 2026-02-27*
