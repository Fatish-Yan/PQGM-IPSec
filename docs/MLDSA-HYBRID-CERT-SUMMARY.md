# ML-DSA-65 混合证书方案实现总结

**日期**: 2026-03-02
**状态**: 已完成

---

## 概述

为解决 OpenSSL 3.0.2 不支持 ML-DSA 证书生成的问题，实现了混合证书方案：在标准 X.509 证书的自定义扩展中存储 ML-DSA 公钥。

---

## 实现成果

### 1. 混合证书生成器

**文件**: `scripts/generate_mldsa_hybrid_cert.c`

**功能**:
- 生成 ML-DSA-65 密钥对 (liboqs)
- 生成 ECDSA P-256 占位符密钥
- 创建包含 ML-DSA 公钥扩展的 X.509 证书
- 使用 ECDSA CA 签名证书

**编译**:
```bash
gcc -o scripts/generate_mldsa_hybrid_cert scripts/generate_mldsa_hybrid_cert.c -loqs -lcrypto -lssl
```

**使用**:
```bash
./scripts/generate_mldsa_hybrid_cert initiator initiator.pqgm.test ./certs/mldsa
```

### 2. mldsa 插件更新

**文件**: `strongswan/src/libstrongswan/plugins/mldsa/mldsa_signer.c`

**新增功能**:
- `extract_mldsa_pubkey_from_cert()` - 从证书扩展提取 ML-DSA 公钥
- `set_key()` 支持证书数据作为输入 (长度 > 500 bytes)

**支持的密钥格式**:
| 格式 | 长度 | 说明 |
|------|------|------|
| 原始私钥 | 4032 bytes | 包含嵌入的公钥 |
| 原始公钥 | 1952 bytes | 仅公钥 |
| 证书数据 | > 500 bytes | 从扩展提取公钥 |

### 3. 测试验证

**文件**: `test_mldsa_cert_extraction.c`

**测试结果**: 全部通过 ✅
```
1. 加载证书 ✅
2. 转换 DER 格式 ✅
3. 提取 ML-DSA 公钥 ✅
4. 加载私钥 ✅
5. 签名测试 ✅
6. 验证测试 ✅
```

---

## 技术细节

### 证书结构

```
X.509 v3 证书
├── 版本: v3
├── 签名算法: ecdsa-with-SHA256
├── 颁发者: CN=PQGM-MLDSA-CA
├── 使用者: CN=<name>.pqgm.test
├── 公钥信息 (SubjectPublicKeyInfo):
│   └── ECDSA P-256 (占位符)
├── 扩展:
│   ├── subjectAltName: DNS:<name>.pqgm.test
│   ├── keyUsage: digitalSignature, keyEncipherment
│   ├── extendedKeyUsage: serverAuth, clientAuth
│   └── 1.3.6.1.4.1.99999.1.2: ML-DSA-65 公钥 (1952 bytes)
└── 签名: ECDSA-SHA256
```

### OID 定义

```
OID: 1.3.6.1.4.1.99999.1.2
DER: 06 0A 2B 06 01 04 01 86 8D 1F 01 02

解释:
- 1.3.6.1.4.1 = enterprises (2B 06 01 04 01)
- 99999 = 86 8D 1F (base-128 编码)
- 1.2 = 01 02
```

### 生成的文件

```
docker/initiator/certs/mldsa/
├── mldsa_ca.pem              # CA 证书
├── mldsa_ca_key.pem          # CA 私钥
├── initiator_hybrid_cert.pem # 混合证书
└── initiator_mldsa_key.bin   # ML-DSA 私钥 (4032 bytes)

docker/responder/certs/mldsa/
├── mldsa_ca.pem
├── mldsa_ca_key.pem
├── responder_hybrid_cert.pem
└── responder_mldsa_key.bin
```

---

## IKE_AUTH 认证流程

```
Initiator                              Responder
    │                                     │
    │──── CERT(混合证书) ─────────────────>│
    │    └── 包含 ML-DSA 公钥扩展          │
    │                                     │
    │──── AUTH(ML-DSA 签名) ──────────────>│
    │    └── 使用 ML-DSA 私钥签名          │
    │                                     │
    │                           从证书扩展提取公钥
    │                           验证 ML-DSA 签名
    │                                     │
    │<─── CERT + AUTH (双向认证) ──────────│
    │                                     │
    ▼                                     ▼
```

---

## 与之前实现的对比

| 方案 | 证书格式 | OpenSSL 要求 | 状态 |
|------|---------|-------------|------|
| 原始密钥 | 无证书 | 无 | ✅ 可用 |
| 混合证书 | X.509 + 扩展 | 3.0.2+ | ✅ 可用 |
| 标准 ML-DSA 证书 | X.509 | 3.5+ | ⏳ 待升级 |

---

## 相关文件

| 文件 | 说明 |
|------|------|
| `scripts/generate_mldsa_hybrid_cert.c` | 混合证书生成器源码 |
| `scripts/generate_mldsa_hybrid_cert` | 编译后的生成器 |
| `test_mldsa_cert_extraction.c` | 证书提取测试源码 |
| `test_mldsa_cert_extraction` | 编译后的测试程序 |
| `docs/plans/2026-03-02-mldsa-cert-extension-design.md` | 设计文档 |
| `strongswan/.../mldsa/mldsa_signer.c` | 插件实现 (支持证书提取) |

---

## Git 提交

```
84bdd30 feat(mldsa): implement ML-DSA public key extraction from certificate extension
```

---

## 后续工作

1. **集成到 IKE_AUTH 流程** - 修改 strongSwan 认证代码使用混合证书
2. **Docker 容器测试** - 在容器中测试完整的 5-RTT 流程
3. **文档更新** - 更新论文系统实现章节

---

*创建时间: 2026-03-02*
