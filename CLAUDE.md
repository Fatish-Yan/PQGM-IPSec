# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**PQ-GM-IKEv2**: 抗量子与国密融合的 IKEv2/IPSec 协议设计与实现 (硕士毕业设计)

核心创新：
- **混合密钥交换**: 经典 DH + 后量子 KEM (ML-KEM) + 国密 SM2-KEM
- **双证书机制**: SM2 签名证书/加密证书分离 + 后量子认证证书
- **基于 RFC 9242/9370**: 利用 IKE_INTERMEDIATE 和多重密钥交换框架

实现平台：strongSwan 6.0.4 + GmSSL 3.1.3 (gmalg 插件)

详细项目文档见：`PROJECT.md`

## Build Commands

### Build GmSSL (required dependency)
```bash
cd GmSSL
mkdir build && cd build
cmake ..
make
sudo make install
```
GmSSL 安装到 `/usr/local` (libgmssl.so, headers in `/usr/local/include/gmssl/`)

### Build strongSwan with gmalg plugin
```bash
cd strongswan
./autogen.sh
./configure --enable-gmalg --enable-swanctl --with-gmssl=/usr/local
make -j$(nproc)
sudo make install
```

### Run tests
```bash
cd /home/ipsec/PQGM-IPSec
LD_LIBRARY_PATH=/usr/local/lib:/home/ipsec/strongswan/src/libstrongswan/.libs \
./test_gmalg          # Test SM3/SM4 functionality
./benchmark_gmalg     # Performance benchmark
```

## Key Paths

```
strongSwan 源码:    /home/ipsec/strongswan
gmalg 插件:         /home/ipsec/strongswan/src/libstrongswan/plugins/gmalg
项目文档:           /home/ipsec/PQGM-IPSec
参考文档:           /home/ipsec/PQGM-IPSec/参考文档/
GmSSL 安装:         /usr/local/lib
```

## System Configuration

```bash
Sudo 密码: 1574a
```

## Architecture

### gmalg Plugin Structure
```
gmalg/
├── gmalg_plugin.c/h      # Plugin entry, algorithm registration
├── gmalg_hasher.c/h      # SM3 hash + PRF
├── gmalg_crypter.c/h     # SM4 ECB/CBC (CTR planned)
├── gmalg_signer.c/h      # SM2 signature
└── Makefile.am
```

### Algorithm IDs (Private Use Range)
```c
#define HASH_SM3        1032    // SM3 Hash
#define ENCR_SM4_ECB    1040    // SM4 ECB mode
#define ENCR_SM4_CBC    1041    // SM4 CBC mode
#define ENCR_SM4_CTR    1042    // SM4 CTR mode
#define AUTH_SM2        1050    // SM2 Signature
#define KE_SM2          1051    // SM2-KEM
#define PRF_SM3         1052    // SM3 PRF
```

### Plugin Registration Pattern
```c
// In gmalg_plugin.c get_features()
// CRYPTER needs 3 args (type, algo, keysize)
PLUGIN_REGISTER(CRYPTER, gmalg_sm4_crypter_create),
    PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_ECB, 16),

// SIGNER needs 2 args only
PLUGIN_REGISTER(SIGNER, gmalg_sm2_signer_create),
    PLUGIN_PROVIDE(SIGNER, AUTH_SM2),
```

## GmSSL 3.1.3 API (Important!)

**GmSSL 3.1.3 API 与旧版本不同，以下为正确用法：**

### SM3 Hash
```c
SM3_CTX ctx;
sm3_init(&ctx);
sm3_update(&ctx, data, len);
sm3_finish(&ctx, digest);
// 注意: sm3_hash() 函数不存在！
```

### SM4 Cipher
```c
SM4_KEY enc_key, dec_key;
sm4_set_encrypt_key(&enc_key, key);
sm4_set_decrypt_key(&dec_key, key);
sm4_encrypt_blocks(&enc_key, in, nblocks, out);
sm4_decrypt_blocks(&dec_key, in, nblocks, out);
```

### SM2 Signature
```c
SM2_KEY sm2_key;
memset(&sm2_key, 0, sizeof(SM2_KEY));  // 注意: sm2_key_init() 不存在！
// 用 sm2_private_key_info_from_der() 或 sm2_public_key_info_from_der() 加载密钥
sm2_sign(&sm2_key, dgst, sig, &siglen);
sm2_verify(&sm2_key, dgst, sig, siglen);
```

**重要注意事项：**
- `sm3_hash()` 函数不存在，必须用 SM3_CTX
- `sm2_key_init()` 函数不存在，用 memset 初始化
- 常量名是 `SM2_DEFAULT_ID_LENGTH` (不是 `SM2_DEFAULT_ID_LEN`)
- 不要包含 `<gmssl/mem.h>` (与 strongSwan 的 memxor 冲突)
- `sm2_key_set_private_key()` 参数类型是 `const uint64_t*`

## Common Errors and Fixes

### 1. HAVE_GMSSL 未定义
手动在 `/home/ipsec/strongswan/config.h` 添加：
```c
#define HAVE_GMSSL 1
```

### 2. PLUGIN_PROVIDE 参数数量
- CRYPTER: 3 参数 `(type, algo, keysize)`
- SIGNER: 2 参数 `(type, algo)`
- HASHER/PRF: 2 参数

### 3. 单体模式编译
Makefile.am 使用条件 LIBADD：
```makefile
if MONOLITHIC
libstrongswan_gmalg_la_LIBADD = -L/usr/local/lib -lgmssl
else
libstrongswan_gmalg_la_LIBADD = $(top_builddir)/src/libstrongswan/libstrongswan.la -lgmssl
endif
```

### 4. 宏定义中的注释
不要在宏定义行内使用 `/* comment */`，会导致 INIT 宏出错

## Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| SM3 Hash | ✅ Done | 443 MB/s |
| SM3 PRF | ✅ Done | 3.7M ops/s |
| SM4 ECB | ✅ Done | 189 MB/s |
| SM4 CBC | ✅ Done | 175 MB/s |
| SM4 CTR | ✅ Done | Functionality tested |
| SM2 Signer | ✅ Done | DER encoding verified |
| SM2-KEM | ✅ Done | Simplified (temp key pairs), 140 bytes ciphertext |
| IKE_INTERMEDIATE | ✅ Verified | ML-KEM tested (3 RTT, +4ms) |
| ML-KEM integration | ✅ Verified | Working with strongSwan 6.0 ml plugin |

## References

- RFC 7296: IKEv2
- RFC 9242: IKE_INTERMEDIATE
- RFC 9370: Multiple Key Exchanges
- FIPS 203: ML-KEM
- GM/T 0002-0004-2012: SM2/SM3/SM4
- strongSwan 6.0 docs: https://docs.strongswan.org/

## Protocol Design (from draft--pqc-gm-ikev2-03.md)

```
IKE_SA_INIT:
  KE=x25519, ADDKE1=ml-kem-768, ADDKE2=sm2-kem
IKE_INTERMEDIATE #0: 双证书分发 (SignCert, EncCert)
IKE_INTERMEDIATE #1: SM2-KEM 密钥交换
IKE_INTERMEDIATE #2: ML-KEM 密钥交换
IKE_AUTH: 后量子签名认证 (ML-DSA/SLH-DSA)
```

双证书机制：
- SignCert: 签名证书 (SM2) - IKE_INTERMEDIATE 阶段
- EncCert: 加密证书 (SM2) - 用于 SM2-KEM
- AuthCert: 认证证书 (ML-DSA/SLH-DSA) - IKE_AUTH 阶段
