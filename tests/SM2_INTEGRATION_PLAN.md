# SM2 国密算法集成方案研究

## 1. SM2 算法概述

### 1.1 SM2 算法标准

SM2（GM/T 0003-2012）是中国国家密码管理局发布的椭圆曲线公钥密码算法，包含：

- **SM2 数字签名算法**：用于身份认证和数字签名
- **SM2 密钥交换协议**：用于密钥协商
- **SM2 公钥加密算法**：用于数据加密

### 1.2 SM2 技术参数

| 参数 | 值 |
|------|-----|
| 椭圆曲线 | sm2p256v1 |
| 基点阶数 | n = FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123 |
| 私钥长度 | 256 bits (32 bytes) |
| 公钥长度 | 512 bits (64 bytes) |
| 签名输出 | 128 bytes (r + s, 各64 bytes) |
| 密文输出 | 97 bytes (C1 + C2 + C3) |

### 1.3 SM2 与其他算法对比

| 算法 | 密钥长度 | 安全强度 | 用途 |
|------|---------|---------|------|
| RSA-2048 | 2048 bits | 112 bits | 签名、加密 |
| ECDSA/P-256 | 256 bits | 128 bits | 签名 |
| ECDH/P-256 | 256 bits | 128 bits | 密钥交换 |
| **SM2** | **256 bits** | **128 bits** | **签名、加密、密钥交换** |
| ML-KEM-768 | 768 bits | 192 bits (量子安全) | 密钥封装 |

---

## 2. 现有实现参考

### 2.1 GmSSL 项目

**GmSSL** 是开源的国密算法工具包，基于 OpenSSL 分支开发。

- **官方网站**: https://gmssl.org/
- **GitHub**: https://github.com/guanzhi/GmSSL
- **Gitee**: https://gitee.com/zhaoxm/GmSSL/

**支持算法**:
- SM2 (椭圆曲线密码)
- SM3 (哈希算法)
- SM4 (对称加密)
- SM9 (基于身份的密码)
- ZUC (序列密码)

**特点**:
- 与 OpenSSL API 兼容
- BSD 开源协议
- 提供命令行工具和 C 库

### 2.2 strongSwan gmalg 插件

已有多个开发者将 SM2/SM3/SM4 集成到 strongSwan：

| 项目 | 链接 | 说明 |
|------|------|------|
| zhangke5959/strongswan | https://github.com/zhangke5959/strongswan | 原始实现 |
| highland0971/strongswan-gmalg-merge | https://github.com/highland0971/strongswan-gmalg-merge | 合并到最新 master |
| lynchen/strongswan-gmalg | https://github.com/lynchen/strongswan-gmalg | 包含测试脚本 |

**实现功能**:
- gmalg plugin（软件实现 SM2/SM3/SM4）
- 修改 pki 工具支持 SM2 证书
- 添加 crypto 测试命令
- 支持 TUN 和 xfrm 模式 IPSec
- 可选 soft_alg 内核驱动

**编译命令**:
```bash
./autogen.sh
./configure --enable-gmalg --prefix=/usr
make && sudo make install
```

---

## 3. 集成方案设计

### 3.1 技术路线对比

| 方案 | 优点 | 缺点 | 推荐度 |
|------|------|------|--------|
| **方案A**: 直接使用 gmalg 插件 | 已有实现，快速集成 | 版本较老，无 ML-KEM | ⭐⭐⭐ |
| **方案B**: GmSSL + strongSwan 6.0.4 | API 兼容性好 | 需要适配工作 | ⭐⭐⭐⭐ |
| **方案C**: 从 gmalg 移植到 6.0.4 | 兼容最新版本 | 开发工作量大 | ⭐⭐ |
| **方案D**: 纯软实现（Botan） | 灵活性高 | 性能较低 | ⭐⭐ |

### 3.2 推荐方案：方案 B（GmSSL + strongSwan 6.0.4）

**理由**:
1. GmSSL 与 OpenSSL API 兼容，strongSwan 6.0.4 已使用 OpenSSL 3.0
2. 只需在现有基础上添加 SM2/SM3/SM4 支持
3. 保留 ML-KEM 插件能力
4. 可逐步实现 PQ-GM-IKEv2 完整协议

### 3.3 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    strongSwan 6.0.4                         │
├─────────────────────────────────────────────────────────────┤
│  charon (IKE daemon)                                        │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              Plugin Architecture                      │  │
│  ├──────────────┬──────────────┬────────────┬───────────┤  │
│  │              │              │            │           │  │
│  │   ml-kem     │   gmalg      │  openssl   │   x509    │  │
│  │   plugin     │   plugin     │  plugin    │  plugin   │  │
│  │              │  (新增)       │  (现有)     │  (现有)    │  │
│  └──────────────┴──────────────┴────────────┴───────────┘  │
│         │              │              │           │         │
│  ┌──────▼──────┐ ┌────▼─────┐ ┌────▼─────┐ ┌──▼──────┐  │
│  │  ML-KEM-768 │ │   SM2    │ │  ECDSA   │ │   RSA   │  │
│  │            │ │   SM3    │ │  ECDH    │ │  DH     │  │
│  └─────────────┘ │   SM4    │ │  SHA2    │ │  AES    │  │
│                 └────┬─────┘ └───────────┘ └─────────┘  │
└───────────────────────┼────────────────────────────────────┘
                        │
                 ┌──────▼──────┐
                 │   GmSSL    │
                 │  (libgmssl)│
                 │  或 OpenSSL│
                 │  + SM2     │
                 └─────────────┘
```

---

## 4. 实现步骤

### 4.1 第一阶段：环境准备

```bash
# 1. 安装 GmSSL
cd /home/ipsec
git clone https://github.com/guanzhi/GmSSL.git
cd GmSSL
./config
make
sudo make install

# 2. 验证安装
gmssl version
gmssl sm2keygen -help
```

### 4.2 第二阶段：创建 gmalg 插件

在 strongSwan 6.0.4 中创建 gmalg 插件：

```
src/libstrongswan/plugins/gmalg/
├── gmalg_plugin.c          # 插件入口
├── gmalg_signer.c          # SM2 签名器
├── gmalg_verifier.c        # SM2 验签器
├── gmalg_crypter.c         # SM2 加解密器
├── gmalg_ke.c              # SM2 密钥交换
├── gmalg_hasher.c          # SM3 哈希器
├── gmalg_crypter.c         # SM4 加解密器
└── Makefile.am
```

### 4.3 第三阶段：实现 SM2 双证书机制

根据 PQ-GM-IKEv2 草案，需要实现：

```c
// 双证书结构
struct gm_cert_config {
    certificate_t *sign_cert;    // SM2 签名证书
    certificate_t *enc_cert;     // SM2 加密证书
    private_key_t *sign_key;     // SM2 签名私钥
    private_key_t *enc_key;      // SM2 加密私钥
};

// IKE_INTERMEDIATE 扩展
// 用于传输加密证书
struct ike_intermediate_cert {
    payload_type_t type;         // CERT payload
    cert_encoding_t encoding;    // X.509
    chunk_t cert_data;           // 加密证书
};
```

### 4.4 第四阶段：SM2-KEM 密钥交换

```c
// SM2-KEM 密钥封装
struct sm2_kem_encapsulate {
    // 1. 生成随机对称密钥 K
    uint8_t K[32];

    // 2. 使用 SM2 加密 K
    sm2_ciphertext_t C;          // C1 || C2 || C3 (97 bytes)

    // 3. 派生 IKE 密钥
    prf_plus(K, "SM2-KEM", dh_secret);
};

// 在 IKE_INTERMEDIATE 中传输 SM2 密文
payload_t kem_payload = {
    .type = KE,
    .transform = {
        .type = KE2_SM2_KEM,
        .data = sm2_ciphertext
    }
};
```

---

## 5. 与 ML-KEM 混合方案

### 5.1 混合密钥交换流程

```
Initiator                                    Responder
    │                                           │
    │── IKE_SA_INIT (KE1: x25519 + ML-KEM-768)──│
    │←──────────────────────────────────────────│
    │                                           │
    │── IKE_INTERMEDIATE (KE2: ML-KEM ct) ──────│
    │←──────────────────────────────────────────│
    │                                           │
    │── IKE_INTERMEDIATE (KE3: SM2-KEM ct) ─────│
    │←──────────────────────────────────────────│
    │                                           │
    │── IKE_AUTH (SM2 SignCert, SM3 Signature) ─│
    │←──────────────────────────────────────────│
    │                                           │
    │   SK = PRF(x25519 || ML-KEM || SM2-KEM)   │
```

### 5.2 密钥材料组合

```c
// 混合密钥派生
struct hybrid_key_material {
    // 传统 ECDH
    chunk_t x25519_secret;

    // 后量子 KEM
    chunk_t mlkem768_shared;

    // 国密 KEM
    chunk_t sm2kem_shared;

    // 组合方式
    chunk_t combined = x25519_secret || mlkem768_shared || sm2kem_shared;
};
```

---

## 6. 配置示例

### 6.1 证书生成

```bash
# 生成 SM2 签名密钥对
gmssl sm2keygen -out sign.key.pem

# 生成 SM2 加密密钥对
gmssl sm2keygen -out enc.key.pem

# 生成签名证书
gmssl sm2certgen -CA ca.crt.pem -CAkey ca.key.pem \
    -in sign.key.pem -out sign.crt.pem \
    -subj "/C=CN/O=PQGM/CN=initiator-sign" \
    -type sign

# 生成加密证书
gmssl sm2certgen -CA ca.crt.pem -CAkey ca.key.pem \
    -in enc.key.pem -out enc.crt.pem \
    -subj "/C=CN/O=PQGM/CN=initiator-enc" \
    -type enc
```

### 6.2 swanctl 配置

```conf
connections {
    pqgm-full {
        proposals = sm2-ke1-x25519-ke2-mlkem768-ke3-sm2kem-aes256-sha256-sm3
        local {
            auth = pubkey
            # 双证书配置
            certs = sign.crt.pem
            enc_certs = enc.crt.pem
            id = "C=CN, O=PQGM, CN=initiator"
        }
        remote {
            auth = pubkey
            id = "C=CN, O=PQGM, CN=responder"
        }
        children {
            net {
                esp_proposals = sm4-aes256-x25519-mlkem768
                start_action = start
            }
        }
    }
}
```

---

## 7. 开发计划

### 7.1 任务分解

| 阶段 | 任务 | 预计时间 | 依赖 |
|------|------|---------|------|
| 1 | 安装 GmSSL，测试 SM2 功能 | 1天 | - |
| 2 | 创建 gmalg 插件框架 | 2天 | 1 |
| 3 | 实现 SM3 哈希器 | 1天 | 2 |
| 4 | 实现 SM2 签名/验签器 | 3天 | 2,3 |
| 5 | 实现 SM4 加解密器 | 2天 | 2,3 |
| 6 | 实现双证书机制 | 3天 | 4 |
| 7 | 实现 SM2-KEM | 4天 | 4,6 |
| 8 | IKE_INTERMEDIATE 扩展 | 3天 | 6,7 |
| 9 | 集成测试 | 2天 | 1-8 |

### 7.2 风险与应对

| 风险 | 影响 | 应对措施 |
|------|------|---------|
| GmSSL API 变化 | 中 | 使用稳定版本，锁定依赖 |
| SM2-KEM 标准不明确 | 高 | 参考现有论文，灵活设计 |
| 性能问题 | 中 | 优化算法，考虑硬件加速 |
| 兼容性问题 | 低 | 充分测试，保持向后兼容 |

---

## 8. 参考资料

### 8.1 标准文档

- [GB/T 32918-2016 SM2 椭圆曲线公钥密码算法](https://openstd.samr.gov.cn/bzgk/gb/newGbInfo?hcno=2A3F6C8B8F4B8D8E8F8F8F8F8F8F8F8F8)
- [GM/T 0003-2012 SM2 密码算法使用规范](https://www.gmbz.org.cn/)
- [RFC 9370: Multiple Key Exchanges in IKEv2](https://datatracker.ietf.org/doc/rfc9370/)

### 8.2 开源项目

- GmSSL: https://github.com/guanzhi/GmSSL
- zhangke5959/strongswan: https://github.com/zhangke5959/strongswan
- Botan (支持 SM2): https://botan.randombit.net/

### 8.3 学术论文

- 《基于 SM2 的 IKEv2 认证与密钥交换协议设计》
- 《抗量子 IKEv2 协议研究进展》

---

## 9. 总结

本研究提出了在 strongSwan 6.0.4 中集成 SM2 国密算法的完整方案，包括：

1. **技术选型**: 采用 GmSSL + strongSwan 的组合方案
2. **架构设计**: 新增 gmalg 插件，保留 ML-KEM 能力
3. **实现路径**: 分 9 个阶段逐步实现
4. **混合方案**: 支持 x25519 + ML-KEM-768 + SM2-KEM 三重密钥交换

下一步可以开始第一阶段的开发工作：安装 GmSSL 并测试基本功能。
