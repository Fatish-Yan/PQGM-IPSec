# ML-DSA 公钥证书扩展存储方案

**日期**: 2026-03-02
**状态**: 设计中

---

## 概述

本方案通过在标准 X.509 证书的自定义扩展中存储 ML-DSA 公钥，实现在 OpenSSL 3.0.2 环境下使用 ML-DSA 进行 IKE_AUTH 认证。

---

## 证书结构设计

### 混合证书结构

```
X.509 v3 证书
├── 版本: v3
├── 序列号: 随机
├── 签名算法: ecdsa-with-SHA256 (CA 签名)
├── 颁发者: CN=PQGM-MLDSA-CA
├── 有效期: 1 年
├── 使用者: CN=initiator.pqgm.test
├── 公钥信息 (SubjectPublicKeyInfo):
│   ├── 算法: id-ecPublicKey (ECDSA P-256)
│   └── 公钥: 65 bytes (占位符，用于证书链验证)
├── 扩展:
│   ├── subjectAltName: DNS:initiator.pqgm.test
│   ├── keyUsage: digitalSignature
│   ├── extendedKeyUsage: serverAuth, clientAuth
│   └── 1.3.6.1.4.1.99999.1.1 (ML-DSA-65 公钥扩展):
│       └── ML-DSA-65 公钥 (1952 bytes, OCTET STRING)
└── 签名: ECDSA-SHA256 (由 CA 签名)
```

### OID 定义

```
ML-DSA 公钥扩展 OID: 1.3.6.1.4.1.99999.1.1

解释:
- 1.3.6.1.4.1: enterprises (企业 OID 分支)
- 99999: 私有企业编号 (PQGM 项目)
- 1: PQGM 扩展
- 1: ML-DSA-65 公钥

变体:
- 1.3.6.1.4.1.99999.1.1: ML-DSA-44 公钥
- 1.3.6.1.4.1.99999.1.2: ML-DSA-65 公钥
- 1.3.6.1.4.1.99999.1.3: ML-DSA-87 公钥
```

---

## 认证流程

### IKE_AUTH 阶段

```
Initiator                              Responder
    │                                     │
    │──── CERT(混合证书) ─────────────────>│
    │    └── 包含 ML-DSA 公钥扩展          │
    │                                     │
    │──── AUTH(ML-DSA 签名) ──────────────>│
    │    └── 使用 ML-DSA 私钥签名          │
    │                                     │
    │                           提取 ML-DSA 公钥
    │                           从证书扩展
    │                                     │
    │                           验证 ML-DSA 签名
    │                                     │
    │<─── CERT + AUTH (双向认证) ──────────│
    │                                     │
    ▼                                     ▼
```

### 密钥使用分离

| 密钥 | 用途 | 来源 |
|------|------|------|
| ECDSA P-256 公钥 | 证书链验证 | SubjectPublicKeyInfo |
| ML-DSA-65 公钥 | AUTH 签名验证 | 自定义扩展 |
| ML-DSA-65 私钥 | AUTH 签名 | 原始密钥文件 |

---

## 实现计划

### 阶段 1: 证书生成脚本

创建 `scripts/generate_mldsa_hybrid_cert.sh`:
1. 生成 ML-DSA-65 密钥对 (liboqs)
2. 生成 ECDSA P-256 密钥对 (占位符)
3. 创建证书扩展配置文件
4. 使用 OpenSSL 生成包含 ML-DSA 公钥扩展的证书

### 阶段 2: mldsa 插件修改

修改 `mldsa_signer.c`:
1. 添加 `set_key_from_cert()` 方法
2. 解析证书的扩展字段
3. 提取 ML-DSA 公钥 (OID: 1.3.6.1.4.1.99999.1.2)
4. 验证公钥大小 (1952 bytes)

### 阶段 3: IKE_AUTH 集成

修改 `ike_auth.c` 或相关认证代码:
1. 加载混合证书
2. 从证书扩展提取 ML-DSA 公钥
3. 使用 ML-DSA 私钥签名
4. 验证对端的 ML-DSA 签名

---

## 安全考虑

1. **证书链验证**: 仍然使用 ECDSA 签名验证证书链
2. **密钥分离**: ECDSA 用于证书验证，ML-DSA 用于 AUTH
3. **扩展验证**: 必须验证扩展中的公钥大小和格式
4. **降级攻击**: 混合证书不提供量子安全证书链

---

## 优势

- ✅ 兼容 OpenSSL 3.0.2
- ✅ 标准证书格式
- ✅ 支持证书链验证
- ✅ 灵活扩展

## 局限性

- ⚠️ 非标准方案
- ⚠️ 需要 strongSwan 修改支持
- ⚠️ 证书链不是后量子安全的
