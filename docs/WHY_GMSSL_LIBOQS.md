# 为什么选择 GmSSL 和 liboqs 而不是 OpenSSL？

## 毕业答辩备问：密码库选型说明

---

## 一、项目使用的密码库版本

| 密码库 | 版本 | 用途 |
|--------|------|------|
| **GmSSL** | 3.1.1 | 国密算法支持 (SM2/SM3/SM4) |
| **liboqs** | 0.12.0 | 后量子密码算法支持 (ML-KEM/ML-DSA) |
| **OpenSSL** | 3.0.2 | 基础加密功能 (系统自带) |

---

## 二、OpenSSL 3.0.2 的局限性

### 2.1 不支持国密算法

**问题**: OpenSSL 3.0.2 原生不支持中国国密算法标准。

| 算法 | OpenSSL 3.0.2 | GmSSL 3.1.1 |
|------|--------------|-------------|
| **SM2** (椭圆曲线签名/密钥交换) | ❌ 不支持 | ✅ 完整支持 |
| **SM3** (杂凑算法) | ❌ 不支持 | ✅ 完整支持 |
| **SM4** (分组密码) | ❌ 不支持 | ✅ 完整支持 |

**影响**: 论文要求实现"国密融合的 IKEv2 协议"，必须支持国密算法。

### 2.2 不支持 SM2 证书的解析

**问题**: 即使通过 provider 方式添加 SM2 算法，OpenSSL 的 X.509 证书解析器也无法正确处理 SM2 证书的 OID。

**具体表现**:
```c
// strongSwan 的 X.509 解析器使用 OpenSSL
// 遇到 SM2 OID 时无法识别
OID_ec_public_key = 18  // 算法 OID
OID_sm2 = 5             // 曲线参数 OID

// OpenSSL 无法解析这种组合，导致证书解析失败
```

**项目中的解决方案**:
```c
// ike_cert_post.c:process_sm2_certs()
// 使用 GmSSL 的 API 直接解析 SM2 证书
x509_cert_get_pubkey = dlsym(gmssl_handle, "x509_cert_get_subject_public_key");
x509_cert_check = dlsym(gmssl_handle, "x509_cert_check");

// 检查 OID 条件
if (x509_key.algor == 18 && x509_key.algor_param == 5)
{
    // 正确识别 SM2 公钥
}
```

### 2.3 后量子密码支持不完整

**问题**: OpenSSL 3.0.2 对后量子密码算法的支持有限。

| 算法 | OpenSSL 3.0.2 | liboqs 0.12.0 |
|------|--------------|---------------|
| **ML-KEM-768** | ❌ 不支持 | ✅ 完整支持 |
| **ML-DSA-65** | ❌ 不支持 | ✅ 完整支持 |
| **SLH-DSA** | ❌ 不支持 | ✅ 完整支持 |

**注**: OpenSSL 3.2+ 开始实验性支持部分后量子算法，但仍不完整。

### 2.4 ML-DSA 证书生成不支持

**问题**: OpenSSL 3.0.2 无法生成包含 ML-DSA 公钥的 X.509 证书。

**项目中的影响**:
- 无法直接生成标准的 ML-DSA 证书
- 需要采用"混合证书"方案：ECDSA P-256 占位符 + ML-DSA 扩展

**替代方案**（项目采用）:
```c
// 使用 liboqs 生成 ML-DSA 密钥对
OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
OQS_SIG_keypair(sig, pubkey, privkey);

// 手动构建证书扩展
// OID: 1.3.6.1.4.1.99999.1.2 (自定义 ML-DSA 扩展)
```

---

## 三、为什么选择 GmSSL？

### 3.1 国密算法的完整实现

**GmSSL 3.1.1 优势**:
1. **原生支持 SM2/SM3/SM4**，无需额外 provider
2. **支持 SM2 双证书机制**（签名证书 + 加密证书）
3. **支持 X.509 SM2 证书解析**，能正确识别 SM2 OID
4. **北京大学 GuanZhi 老师维护**，国产自主可控

**项目中使用场景**:
```c
// gmalg_ke.c: SM2-KEM 密钥封装
#include <gmssl/sm2.h>
#include <gmssl/sm2_z256.h>

// SM2 加密（封装）
sm2_encrypt(&peer_key, plaintext, ptlen, ciphertext, &ctlen);

// SM2 解密（解封装）
sm2_decrypt(&my_key, ciphertext, ctlen, plaintext, &ptlen);
```

### 3.2 与 strongSwan 的集成

**GmSSL 可以无缝替换 OpenSSL**:
```bash
./configure --with-gmssl=/usr/local
```

**原因**: GmSSL 保持与 OpenSSL API 兼容，大部分函数签名相同。

### 3.3 符合国密合规要求

**论文要求**: 实现"国密融合的 IKEv2 协议"

**GmSSL 满足**:
- GM/T 0002-2012 (SM2)
- GM/T 0003-2012 (SM3)
- GM/T 0004-2012 (SM4)
- GM/T 0009-2012 (SM2 证书规范)

---

## 四、为什么选择 liboqs？

### 4.1 后量子密码的标准实现

**liboqs 0.12.0 优势**:
1. **NIST 后量子标准化算法参考实现**
2. **支持 ML-KEM (Kyber)** - FIPS 203 标准
3. **支持 ML-DSA (Dilithium)** - FIPS 204 标准
4. **支持 SLH-DSA (Sphincs+)** - FIPS 205 标准
5. **OpenQuantumSafe 组织维护**，业界公认

### 4.2 与 strongSwan 的集成

**strongSwan 6.0+ 原生支持 liboqs**:
```bash
./configure --enable-mldsa --with-liboqs=/path/to/liboqs
```

**项目中使用场景**:
```c
// mldsa_signer.c: ML-DSA-65 签名
#include <oqs/oqs.h>

// 创建签名上下文
OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);

// 签名验证
OQS_SIG_verify(sig, message, msg_len, signature, sig_len, pubkey);
```

### 4.3 符合 NIST 后量子标准

**论文要求**: 实现"抗量子 IPsec 协议"

**liboqs 满足**:
- FIPS 203 (ML-KEM)
- FIPS 204 (ML-DSA)
- FIPS 205 (SLH-DSA)

---

## 五、综合对比表

| 特性 | OpenSSL 3.0.2 | GmSSL 3.1.1 | liboqs 0.12.0 |
|------|---------------|-------------|---------------|
| **SM2 签名** | ❌ | ✅ | ❌ |
| **SM2-KEM** | ❌ | ✅ | ❌ |
| **SM3** | ❌ | ✅ | ❌ |
| **SM4** | ❌ | ✅ | ❌ |
| **SM2 证书解析** | ❌ | ✅ | ❌ |
| **ML-KEM-768** | ❌ | ❌ | ✅ |
| **ML-DSA-65** | ❌ | ❌ | ✅ |
| **X.509 支持** | ✅ | ✅ | ❌ |
| **TLS/SSL 支持** | ✅ | ✅ | ❌ |

---

## 六、项目选型策略

### 6.1 为什么不是"只用 OpenSSL"？

**原因**:
1. OpenSSL 3.0.2 不支持国密算法
2. OpenSSL 3.0.2 不支持后量子算法
3. OpenSSL 的 X.509 解析器不支持 SM2 OID
4. 即使升级到 OpenSSL 3.2+，后量子支持仍不完整

### 6.2 为什么不是"只用 GmSSL"？

**原因**:
1. GmSSL 专注于国密算法
2. GmSSL 不支持后量子算法 (ML-KEM/ML-DSA)
3. 项目需要同时支持国密和后量子

### 6.3 为什么不是"只用 liboqs"？

**原因**:
1. liboqs 只提供后量子算法原语
2. liboqs 不支持 X.509 证书
3. liboqs 不支持 TLS/SSL 协议
4. liboqs 不支持国密算法

### 6.4 最终选型：GmSSL + liboqs + OpenSSL

**组合策略**:
```
┌─────────────────────────────────────────────────────────┐
│                    strongSwan 6.0.4                      │
├─────────────────────────────────────────────────────────┤
│  gmalg 插件 (基于 GmSSL 3.1.1)                           │
│  ├── SM2 签名                                            │
│  ├── SM2-KEM                                             │
│  ├── SM3 Hash/PRF                                        │
│  └── SM4 加密                                            │
├─────────────────────────────────────────────────────────┤
│  mldsa 插件 (基于 liboqs 0.12.0)                         │
│  ├── ML-DSA-65 签名                                      │
│  └── ML-KEM-768 密钥交换                                 │
├─────────────────────────────────────────────────────────┤
│  OpenSSL 3.0.2 (系统基础库)                              │
│  ├── X.509 证书解析 (非 SM2)                             │
│  ├── RSA/ECDSA 签名                                      │
│  └── 基础加密功能                                        │
└─────────────────────────────────────────────────────────┘
```

---

## 七、答辩回答模板

**如果老师问**: "为什么选择 GmSSL 和 liboqs？"

**回答**:

> "本项目选择 GmSSL 和 liboqs 是基于功能需求和合规要求的综合考量：
>
> **第一，国密算法支持**：项目要求实现'国密融合的 IKEv2 协议'，需要支持 SM2/SM3/SM4 算法。OpenSSL 3.0.2 原生不支持这些国密算法，而 GmSSL 3.1.1 提供了完整的国密算法实现，并且支持 SM2 双证书机制和 X.509 SM2 证书解析。
>
> **第二，后量子算法支持**：项目要求实现'抗量子 IPsec 协议'，需要支持 ML-KEM 和 ML-DSA 算法。OpenSSL 3.0.2 不支持这些后量子算法，而 liboqs 0.12.0 是 NIST 后量子标准化算法的参考实现，被业界广泛认可。
>
> **第三，合规要求**：GmSSL 符合国密标准 (GM/T 0002/0003/0004)，liboqs 符合 NIST 后量子标准 (FIPS 203/204/205)，这样的组合能够满足论文的'国密 + 抗量子'双重要求。
>
> **第四，与 strongSwan 的集成**：GmSSL 保持与 OpenSSL API 兼容，可以直接替换；liboqs 被 strongSwan 6.0+ 原生支持。这使得集成工作更加顺畅。
>
> 因此，项目采用'GmSSL + liboqs + OpenSSL'的组合方案，各自发挥优势，共同支撑 PQ-GM-IKEv2 协议的实现。"

---

**文档版本**: 1.0
**更新日期**: 2026-03-07
**用途**: 硕士毕业答辩备问资料
