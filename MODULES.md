# PQ-GM-IKEv2 模块拆分方案

> 创建时间: 2026-02-26
> 目的: 支持多 agent 并行开发

---

## 模块总览

```
┌─────────────────────────────────────────────────────────────────┐
│                        PQ-GM-IKEv2 系统                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │  Module 1   │  │  Module 2   │  │  Module 3   │             │
│  │  基础算法   │  │  SM2-KEM    │  │  证书机制   │             │
│  │  (已完成)   │  │  (进行中)   │  │  (待开发)   │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │  Module 4   │  │  Module 5   │  │  Module 6   │             │
│  │  ML-KEM     │  │  协议集成   │  │  测试评估   │             │
│  │  (配置)     │  │  (待开发)   │  │  (待开发)   │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Module 1: 基础国密算法 (已完成 ✅)

**状态**: 已完成，仅需维护

**文件**:
- `gmalg_hasher.c/h` - SM3 Hash
- `gmalg_crypter.c/h` - SM4 ECB/CBC/CTR
- `gmalg_signer.c/h` - SM2 Signature
- `gmalg_prf.c/h` - SM3 PRF

**性能数据** (已收集):
| 算法 | 性能 |
|------|------|
| SM3 Hash | 443 MB/s |
| SM3 PRF | 3.7M ops/s |
| SM4 ECB | 189 MB/s |
| SM4 CBC | 175 MB/s |
| SM4 CTR | 待测 |

**负责 Agent**: 无需分配 (已完成)

---

## Module 2: SM2-KEM 密钥交换 (已完成 ✅)

**状态**: 双向封装实现完成

**文件**:
- `gmalg_ke.c/h` - SM2-KEM 实现

**已完成任务**:
- [x] 实现 `ke_t` 接口
- [x] 双向封装 (r_i || r_r)
- [x] 预分配密钥支持 (模拟 R0 证书交换)
- [x] 注册到插件 (Transform ID: KE_SM2 = 1051)

**API**:
```c
// 默认创建 (Initiator 角色)
key_exchange_t* gmalg_sm2_ke_create(key_exchange_method_t method);

// 指定角色创建
key_exchange_t* gmalg_sm2_ke_create_with_role(method, is_initiator);

// 预分配密钥创建 (模拟 R0 完成)
key_exchange_t* gmalg_sm2_ke_create_with_keys(method, is_initiator, my_key, peer_pubkey);
```

**测试结果**:
- 所有测试通过 ✅
- 共享密钥: 64 字节 (r_i || r_r)
- 密文: 141 字节 (DER 编码)

**负责 Agent**: 已完成

---

## Module 3: 证书机制 (部分完成 ✅⏳)

**状态**: M3.1 已完成，M3.2 待开发

### 3.1 双证书生成 (已完成 ✅)

**完成内容**:
- [x] SM2 CA 证书 (`certs/ca/ca_sm2_cert.pem`)
- [x] SM2 签名密钥对 + 证书 (SignCert) - initiator & responder
- [x] SM2 加密密钥对 + 证书 (EncCert) - initiator & responder
- [x] SPHINCS+ 认证密钥对 (AuthKey) - 原始格式

**生成脚本**: `scripts/gen_certs.sh`
**文档**: `certs/README.md`
**证书密码**: `PQGM2026`

**证书属性**:
| 证书类型 | Key Usage | 算法 |
|---------|-----------|------|
| SignCert | digitalSignature, nonRepudiation | SM2-with-SM3 |
| EncCert | keyEncipherment, dataEncipherment | SM2-with-SM3 |
| AuthKey | - | SPHINCS+-SM3 (raw) |

### 3.2 证书分发机制 (待开发 ⏳)
**任务**:
- [ ] IKE_INTERMEDIATE #0 阶段发送双证书
- [ ] 证书解析与存储
- [ ] 将 SPHINCS+ 密钥包装为 X.509 证书

**依赖**:
- strongSwan 证书管理模块
- 需要深入理解 charon 任务流

**负责 Agent**: 需要 strongSwan 内部架构专家

---

## Module 4: ML-KEM 集成 (配置级)

**状态**: 基础功能已验证，需配置集成

**任务**:
- [ ] 配置 strongSwan ml 插件
- [ ] swanctl.conf 配置文件编写
- [ ] 与 x25519 + SM2-KEM 组合配置

**参考**:
- `pqgm-test/results/final_report.txt` (已验证 ML-KEM-768)

**配置示例**:
```conf
proposals = aes256gcm16-prfsha256-x25519-ke1_mlkem768-ke2_sm2kem
```

**负责 Agent**: 可独立分配 (配置任务)

---

## Module 5: 协议集成 (待开发)

**状态**: 依赖 Module 2, 3, 4

**任务**:
- [ ] IKE_SA_INIT 提案协商
- [ ] IKE_INTERMEDIATE 流程编排
- [ ] 密钥派生链 (RFC 9370)
- [ ] IKE_AUTH 后量子认证

**核心代码位置**:
- `/home/ipsec/strongswan/src/libstrongswan/sa/ike/`
- `/home/ipsec/strongswan/src/libstrongswan/sa/keymat/`

**负责 Agent**: 需要 strongSwan 架构专家

---

## Module 6: 测试与评估 (待开发)

**状态**: 依赖所有模块完成

### 6.1 功能测试
**任务**:
- [ ] 单元测试完善
- [ ] 双机连通性测试
- [ ] 协议完整性验证

### 6.2 性能测试
**任务**:
- [ ] 密钥交换延迟测量
- [ ] 吞吐量测试 (iperf3)
- [ ] CPU/内存占用分析

### 6.3 论文数据收集
**任务**:
- [ ] 对比测试: 标准 IKEv2 vs PQ-GM-IKEv2
- [ ] 生成图表数据
- [ ] 更新论文第5章

**负责 Agent**: 可独立分配

---

## 模块依赖关系

```
Module 1 (基础算法) ─────┬───────────────────────────────────────┐
                         │                                       │
                         ▼                                       │
Module 2 (SM2-KEM) ──────┼───────────────────────────────────────┤
                         │                                       │
Module 3 (证书) ─────────┼───────────────────────────────────────┤
                         │                                       │
Module 4 (ML-KEM) ───────┼───────────────────────────────────────┤
                         │                                       │
                         ▼                                       │
                   Module 5 (协议集成) ◄──────────────────────────┘
                         │
                         ▼
                   Module 6 (测试评估)
```

**并行开发可能性**:
- ✅ Module 2, 3.1, 4 可完全并行
- ⚠️ Module 3.2 需要 strongSwan 内部知识
- ❌ Module 5 需等待 2, 3, 4
- ❌ Module 6 需等待 5

---

## 推荐的 Agent 分配

| Agent | 模块 | 技能要求 | 优先级 |
|-------|------|----------|--------|
| Agent A | Module 2 (SM2-KEM) | GmSSL API, strongSwan KE | 高 |
| Agent B | Module 3.1 (证书生成) | OpenSSL/GmSSL, 证书格式 | 中 |
| Agent C | Module 4 (ML-KEM配置) | strongSwan 配置 | 中 |
| Agent D | Module 5 (协议集成) | strongSwan 架构专家 | 高 |
| Agent E | Module 6 (测试) | 测试工具, 数据分析 | 低 |

---

## 快速启动任务 (可立即分配)

### Task 1: 证书生成脚本
**描述**: 编写脚本生成 SM2 双证书和 ML-DSA 认证证书
**文件**: `scripts/gen_certs.sh` 或 `scripts/gen_certs.py`
**依赖**: 无
**预计时间**: 2-3 小时

### Task 2: ML-KEM 配置文件
**描述**: 编写 swanctl.conf 配置文件，支持 x25519 + ML-KEM-768
**文件**: `configs/swanctl_pqgm.conf`
**依赖**: 无 (使用现有 ml 插件)
**预计时间**: 1-2 小时

### Task 3: 性能测试脚本
**描述**: 编写自动化性能测试脚本
**文件**: `scripts/benchmark.sh`
**依赖**: 无
**预计时间**: 2-3 小时

### Task 4: 文档整理
**描述**: 整理 API 文档、错误记录、测试报告
**文件**: `docs/`
**依赖**: 无
**预计时间**: 1-2 小时

---

## 当前工作分配

| 模块 | 状态 | 负责 Agent | 备注 |
|------|------|-----------|------|
| Module 2 (SM2-KEM) | 进行中 | Agent A (其他对话) | 方案A 简化版 |
| Module 3.1 (证书) | 待分配 | - | 可立即开始 |
| Module 4 (ML-KEM) | 待分配 | - | 可立即开始 |
| Module 6 (测试脚本) | 待分配 | - | 可立即开始 |

---

**下一步**: 请告诉我您想先分配哪些任务？
