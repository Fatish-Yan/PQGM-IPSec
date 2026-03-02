# ML-DSA IKE_AUTH 集成设计文档

**日期**: 2026-03-02
**状态**: 设计完成，待实现

---

## 概述

将 ML-DSA 混合证书方案集成到 strongSwan 的 IKE_AUTH 认证流程中，实现后量子安全的身份认证。

---

## 设计决策

### 认证方式
- **纯 ML-DSA 认证**: 双方都使用 ML-DSA 混合证书进行认证

### 测试方式
- **Docker 容器测试**: 使用现有的 Docker 环境进行测试

### 实现方案
- **配置驱动方案**: 通过 swanctl 配置指定 ML-DSA 认证，最小化代码修改

---

## 架构设计

### IKE_AUTH 认证流程

```
Initiator                              Responder
    │                                     │
    │──── CERT(混合证书) ─────────────────>│
    │     └── ECDSA占位符                  │
    │     └── ML-DSA公钥扩展               │
    │                                     │
    │──── AUTH(ML-DSA签名) ──────────────>│
    │     └── ML-DSA私钥签名               │
    │                                     │
    │                           提取ML-DSA公钥
    │                           验证ML-DSA签名
    │                                     │
    │<─── CERT + AUTH ───────────────────│
    │                                     │
    ▼                                     ▼
```

### 组件依赖关系

```
┌─────────────────┐     ┌─────────────────┐
│  swanctl.conf   │────>│  IKE_AUTH       │
│  (配置)         │     │  (认证处理)     │
└─────────────────┘     └────────┬────────┘
                                 │
                                 ▼
┌─────────────────┐     ┌─────────────────┐
│  混合证书       │────>│  mldsa_signer   │
│  (.pem)         │     │  (签名/验证)    │
└─────────────────┘     └─────────────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  liboqs         │
                        │  (ML-DSA实现)   │
                        └─────────────────┘
```

---

## 组件修改清单

### strongSwan 核心修改

| 文件 | 修改内容 |
|------|---------|
| `src/libcharon/sa/ikev2/tasks/ike_auth.c` | 添加 ML-DSA 认证支持，从混合证书提取公钥 |
| `src/libstrongswan/credentials/keys/private_key.c` | 添加 ML-DSA 私钥加载支持 (原始二进制格式) |
| `src/libstrongswan/plugins/mldsa/mldsa_signer.c` | ✅ 已完成 |

### 配置文件

| 文件 | 说明 |
|------|------|
| `docker/initiator/config/swanctl-mldsa-hybrid.conf` | Initiator 配置 |
| `docker/responder/config/swanctl-mldsa-hybrid.conf` | Responder 配置 |

### 证书/密钥文件 (已生成)

| 文件 | 大小 | 说明 |
|------|------|------|
| `initiator_hybrid_cert.pem` | ~2.5KB | Initiator 混合证书 |
| `initiator_mldsa_key.bin` | 4032 bytes | Initiator ML-DSA 私钥 |
| `responder_hybrid_cert.pem` | ~2.5KB | Responder 混合证书 |
| `responder_mldsa_key.bin` | 4032 bytes | Responder ML-DSA 私钥 |
| `mldsa_ca.pem` | ~1KB | CA 证书 |

---

## 实现步骤

### Step 1: 创建 swanctl 配置文件

创建 `swanctl-mldsa-hybrid.conf`:
```conf
connections {
    pqgm-mldsa {
        remote_addrs = 172.28.0.20

        local {
            auth = pubkey
            certs = initiator_hybrid_cert.pem
            # ML-DSA 私钥需要特殊处理
        }

        remote {
            auth = pubkey
            id = responder.pqgm.test
        }

        children {
            net {
                remote_ts = 10.2.0.0/16
                local_ts = 10.1.0.0/16
                esp_proposals = aes256-sha256
            }
        }

        version = 2
        proposals = aes256-sha256-x25519
    }
}
```

### Step 2: 修改私钥加载支持

需要让 strongSwan 能够加载原始 ML-DSA 私钥文件:
- 方案 A: 修改 private_key.c 支持 .bin 格式
- 方案 B: 创建 ML-DSA 私钥 PEM 包装器
- 方案 C: 在配置中直接指定私钥文件路径

### Step 3: 修改 IKE_AUTH 认证流程

关键修改点:
1. 识别混合证书中的 ML-DSA 扩展
2. 使用 `extract_mldsa_pubkey_from_cert()` 提取公钥
3. 调用 mldsa 插件进行签名/验证

### Step 4: Docker 容器测试

1. 重建 Docker 镜像
2. 部署配置和证书
3. 运行 IKE_AUTH 测试
4. 验证签名格式和大小

### Step 5: 验证和记录

- 抓包验证 ML-DSA 签名 (3309 bytes)
- 记录测试结果
- 更新文档

---

## 测试验证点

| 验证项 | 预期结果 | 状态 |
|--------|---------|------|
| 混合证书加载 | 正确加载 PEM 格式 | ⏳ |
| ML-DSA 公钥提取 | 从扩展中提取 1952 bytes 公钥 | ✅ 已验证 |
| 私钥加载 | 加载 4032 bytes 原始私钥 | ⏳ |
| 签名生成 | 生成 3309 bytes ML-DSA 签名 | ⏳ |
| 签名验证 | 验证对端签名成功 | ⏳ |
| 双向认证 | IKE_AUTH 双向认证成功 | ⏳ |

---

## 成功标准

1. IKE_AUTH 阶段使用 ML-DSA 签名完成双向认证
2. 抓包显示 AUTH payload 使用 ML-DSA-65 签名 (3309 bytes)
3. strongSwan 日志显示 ML-DSA 签名验证成功
4. IPSec SA 建立成功

---

## 风险和缓解措施

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| strongSwan 不支持原始私钥格式 | 高 | 创建私钥加载插件或转换格式 |
| IKE_AUTH 不识别 ML-DSA 算法 | 高 | 修改认证处理逻辑 |
| 证书链验证失败 | 中 | 使用 CA 证书验证 ECDSA 签名 |

---

## 相关文档

- [ML-DSA 混合证书方案总结](../MLDSA-HYBRID-CERT-SUMMARY.md)
- [ML-DSA 证书扩展设计](./2026-03-02-mldsa-cert-extension-design.md)
- [修复记录](../FIXES-RECORD.md)

---

*创建时间: 2026-03-02*
