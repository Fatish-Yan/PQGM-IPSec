# ML-DSA-65 签名插件设计文档

## 概述

**目标**: 为 strongSwan 添加 ML-DSA-65 后量子签名支持，用于 IKE_AUTH 阶段的双向证书认证。

**日期**: 2026-03-02
**状态**: 已批准

---

## 需求决策

| 项目 | 决策 |
|------|------|
| 实现库 | liboqs (Open Quantum Safe) |
| 插件模式 | 独立 `mldsa` 插件，参考 `gmalg` |
| 签名变体 | ML-DSA-65 优先，保留扩展空间 |
| 证书处理 | OpenSSL 解析结构 + liboqs 验证签名 |
| 实现方案 | 最小化签名器插件 |

---

## 架构设计

### IKE_AUTH 双向认证流程 (RFC 7296)

```
Initiator                              Responder
    │                                     │
    │──── IDi, CERT(ML-DSA), AUTH ───────>│  ← Initiator 用 ML-DSA 私钥签名
    │                                     │  ← Responder 用 Initiator 公钥验证
    │                                     │
    │<─── IDr, CERT(ML-DSA), AUTH ────────│  ← Responder 用 ML-DSA 私钥签名
    │                                     │  ← Initiator 用 Responder 公钥验证
    │                                     │
    ▼                                     ▼
```

### 插件架构

```
┌─────────────────────────────────────────────────────────────┐
│                    strongSwan charon                        │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────┐                                       │
│  │  mldsa_signer    │  ← signer_t 接口                      │
│  │  (mldsa_signer.c)│                                       │
│  │       │          │                                       │
│  │       ▼          │                                       │
│  │    liboqs        │  ← OQS_SIG (ML-DSA-65)               │
│  │  (OQS_SIG_sign)  │                                       │
│  │  (OQS_SIG_verify)│                                       │
│  └──────────────────┘                                       │
│                                                             │
│  ┌──────────────────┐                                       │
│  │  openssl plugin  │  ← X.509 证书解析                     │
│  │  (证书结构解析)   │                                       │
│  └──────────────────┘                                       │
└─────────────────────────────────────────────────────────────┘
```

---

## 文件结构

```
strongswan/src/libstrongswan/plugins/mldsa/
├── mldsa_plugin.c      # 插件注册 (PLUGIN_REGISTER/PROVIDE)
├── mldsa_plugin.h
├── mldsa_signer.c      # signer_t 接口实现
├── mldsa_signer.h
├── Makefile.am         # 构建配置
└── (configure.ac 修改) # 添加 --enable-mldsa 选项
```

---

## 核心接口设计

### mldsa_signer.c

```c
/**
 * ML-DSA-65 签名器实现
 * 参考: gmalg_signer.c
 */

struct private_mldsa_signer_t {
    signer_t public;
    OQS_SIG *sig_ctx;           // liboqs 签名上下文
    chunk_t private_key;         // 本端私钥
    chunk_t public_key;          // 对端公钥 (验证时使用)
};

METHOD(signer_t, get_signature, bool,
    private_mldsa_signer_t *this, chunk_t data, chunk_t *signature)
{
    // 1. 计算数据哈希 (SHA-512)
    // 2. 调用 OQS_SIG_sign()
    // 3. 返回签名 (原始格式，非 DER)
}

METHOD(signer_t, verify_signature, bool,
    private_mldsa_signer_t *this, chunk_t data, chunk_t signature)
{
    // 1. 使用对端公钥
    // 2. 调用 OQS_SIG_verify()
    // 3. 返回验证结果
}

METHOD(signer_t, get_key_size, size_t,
    private_mldsa_signer_t *this)
{
    return MLDSA65_PUBLIC_KEY_BYTES;  // 1952 bytes
}

METHOD(signer_t, get_block_size, size_t,
    private_mldsa_signer_t *this)
{
    return HASH_SIZE_SHA512;
}
```

### mldsa_plugin.c

```c
METHOD(plugin_t, get_features, int,
    private_mldsa_plugin_t *this, plugin_feature_t *features[])
{
    static plugin_feature_t f[] = {
        PLUGIN_REGISTER(SIGNER, mldsa_signer_create),
            PLUGIN_PROVIDE(SIGNER, AUTH_MLDSA_65),
    };
    *features = f;
    return countof(f);
}
```

### 认证方法 ID

```c
// 在 authenticator.h 中添加
typedef enum {
    // ... 现有值 ...
    AUTH_ECDSA_521 = 11,
    AUTH_MLDSA_65 = 12,    // 新增
    AUTH_MLDSA_44 = 13,    // 保留扩展
    AUTH_MLDSA_87 = 14,    // 保留扩展
} auth_method_t;
```

---

## liboqs 集成

### 依赖

```bash
# Ubuntu/Debian
sudo apt install liboqs-dev

# 或从源码编译
git clone https://github.com/open-quantum-safe/liboqs.git
cd liboqs
mkdir build && cd build
cmake ..
make
sudo make install
```

### API 使用

```c
#include <oqs/oqs.h>

// 初始化 ML-DSA-65 上下文
OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);

// 签名
OQS_STATUS status = OQS_SIG_sign(
    sig,
    signature, signature_len,
    message, message_len,
    private_key
);

// 验证
OQS_STATUS status = OQS_SIG_verify(
    sig,
    message, message_len,
    signature, signature_len,
    public_key
);

// 清理
OQS_SIG_free(sig);
```

### ML-DSA-65 参数

| 参数 | 大小 |
|------|------|
| 公钥 | 1,952 bytes |
| 私钥 | 4,032 bytes (seed + expanded) |
| 签名 | 3,293 bytes |

---

## 密钥和证书管理

### 密钥存储路径

```
/usr/local/etc/swanctl/
├── private/
│   └── mldsa_key.pem      # ML-DSA-65 私钥
├── x509/
│   ├── mldsa_cert.pem     # ML-DSA-65 证书
│   └── peer_mldsa_cert.pem # 对端 ML-DSA-65 证书
└── x509ca/
    └── mldsa_ca.pem       # ML-DSA-65 CA 证书
```

### 证书生成

使用 liboqs 提供的工具或 OpenSSL 3.5+：

```bash
# 使用 oqs-provider (OpenSSL 3.5+)
openssl req -x509 -newkey ml-dsa-65 \
    -keyout mldsa_key.pem \
    -out mldsa_cert.pem \
    -days 365 \
    -subj "/CN=initiator.pqgm.test" \
    -addext "subjectAltName=DNS:initiator.pqgm.test"
```

### ML-DSA OID

```
ML-DSA-44: 1.3.6.1.4.1.22554.5.6.1
ML-DSA-65: 1.3.6.1.4.1.22554.5.6.2
ML-DSA-87: 1.3.6.1.4.1.22554.5.6.3
```

---

## swanctl 配置

```conf
connections {
    pqgm-ikev2-mldsa {
        version = 2
        local_addrs = 172.28.0.10
        remote_addrs = 172.28.0.20
        proposals = aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768

        local {
            # 使用 ML-DSA 证书认证
            auth = mldsa
            id = initiator.pqgm.test
            certs = mldsa_cert.pem
        }

        remote {
            auth = mldsa
            id = responder.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.1.0.0/16
                remote_ts = 10.2.0.0/16
                esp_proposals = aes256gcm16-sha256
                start_action = none
            }
        }
    }
}

secrets {
    mldsa-initiator {
        id = initiator.pqgm.test
        file = mldsa_key.pem
    }
}
```

---

## 构建配置

### Makefile.am

```makefile
AM_CPPFLAGS = -I$(top_srcdir)/src/libstrongswan -DPLUGIN_NAME=$(plugin_name)

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-mldsa.la
else
plugin_LTLIBRARIES = libstrongswan-mldsa.la
endif

libstrongswan_mldsa_la_SOURCES = \
    mldsa_plugin.c mldsa_plugin.h \
    mldsa_signer.c mldsa_signer.h

libstrongswan_mldsa_la_LDFLAGS = -module -avoid-version
libstrongswan_mldsa_la_LIBADD = -loqs
```

### configure.ac 修改

```m4
# 添加 mldsa 插件选项
ARG_ENABLABLE([mldsa], [enable ML-DSA signature support.])

if test x$mldsa = xtrue; then
    PKG_CHECK_MODULES(liboqs, liboqs)
    AC_DEFINE([HAVE_MLDSA], [], [have ML-DSA support])
fi

AM_CONDITIONAL(USE_MLDSA, [test x$mldsa = xtrue])
```

---

## 测试计划

| 测试项 | 方法 | 预期结果 |
|--------|------|----------|
| liboqs 链接 | 编译测试 | 编译成功 |
| 单元测试 - 签名 | 签名/验证基本功能 | 签名验证通过 |
| 单元测试 - 验证 | 错误签名验证 | 验证失败 |
| 证书加载 | 加载 ML-DSA 证书 | 证书加载成功 |
| IKE_AUTH 流程 | 完整 5-RTT 测试 | IKE_SA 建立成功 |
| IntAuth 绑定 | 验证 AUTH 包含 IntAuth | AUTH 验证通过 |

---

## 风险和缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| liboqs ABI 不稳定 | 编译/运行失败 | 指定 liboqs 版本 (≥0.10) |
| ML-DSA 证书不被 OpenSSL 识别 | 证书加载失败 | 使用文件 fallback 或自定义解析 |
| 性能问题 | 连接延迟增加 | ML-DSA-65 签名 ~2ms，可接受 |

---

## 后续扩展

1. **ML-DSA-44/87 支持**: 添加认证方法 ID 和算法选择
2. **混合签名**: ML-DSA + ECDSA 混合认证
3. **证书链支持**: 完整的 X.509 证书链验证
4. **性能优化**: 缓存 OQS_SIG 上下文

---

## 参考资料

- RFC 7296: Internet Key Exchange Protocol Version 2 (IKEv2)
- FIPS 204: Module-Lattice-Based Digital Signature Standard
- liboqs: https://github.com/open-quantum-safe/liboqs
- strongSwan signer_t 接口: `src/libstrongswan/crypto/signers/signer.h`

---

## Implementation Status

**Date**: 2026-03-02

**Phase 1: Plugin Skeleton** ✅ Complete
- mldsa_plugin.c/h created
- mldsa_signer.c/h created
- Makefile.am created

**Phase 2: Build and Test** ✅ Complete
- Plugin integrated into build system
- Plugin compiles successfully
- Unit tests pass

**Phase 3: Integration** ✅ Complete
- Plugin loads successfully in strongSwan
- AUTH_MLDSA_65 (1053) registered

**Phase 4: End-to-End Testing** ⏳ Deferred
- Pending OpenSSL 3.5+ upgrade for ML-DSA certificate support
- See [MLDSA-IKE-AUTH-TEST-STATUS.md](../../MLDSA-IKE-AUTH-TEST-STATUS.md)

---

*设计批准日期: 2026-03-02*
