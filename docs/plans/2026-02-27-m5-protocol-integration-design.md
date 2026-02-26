# M5 协议集成设计文档

> 创建时间: 2026-02-27
> 状态: 已确认，待实现

---

## 1. 概述

### 1.1 目标
将已完成的 M1-M4 模块集成到完整的 PQ-GM-IKEv2 协议流程中。

### 1.2 已完成模块
| 模块 | 内容 | 状态 |
|------|------|------|
| M1 | SM3/SM4/SM2-Sign 基础算法 | ✅ |
| M2 | SM2-KEM 双向封装 (141字节密文, 64字节共享密钥) | ✅ |
| M3 | 双证书生成 + IKE_INTERMEDIATE #0 证书分发 | ✅ |
| M4 | ML-KEM-768 配置和测试 | ✅ |

### 1.3 协议流程
```
IKE_SA_INIT (协商 KE=x25519, ADDKE1=ml-kem-768, ADDKE2=sm2-kem)
  → IKE_INTERMEDIATE #0 (双证书分发)
  → IKE_INTERMEDIATE #1 (SM2-KEM 密钥交换)
  → IKE_INTERMEDIATE #2 (ML-KEM-768 密钥交换)
  → IKE_AUTH (ML-DSA 后量子认证)
```

---

## 2. 关键设计决策

### 2.1 IKE_SA_INIT 提案协商
**决策**: 强制要求三种密钥交换都支持

**配置语法**:
```bash
# swanctl.conf
connections {
    pqgm-ikev2 {
        version = 2
        proposals = aes256-sha384-x25519-ke1_mlkem768-ke2_sm2kem

        children {
            ipsec {
                esp_proposals = aes256gcm256-x25519-ke1_mlkem768-ke2_sm2kem
            }
        }
    }
}
```

**Transform 映射**:
- Transform 4 (KEY_EXCHANGE): x25519
- Transform 6 (ADDKE1): ml-kem-768
- Transform 7 (ADDKE2): sm2-kem

### 2.2 ADDKE 执行顺序
**决策**: strongSwan 自动按 ADDKE1→ADDKE2 顺序执行

**原理**: RFC 9370 定义了 ADDKE1..ADDKE7，strongSwan 按顺序自动执行。

**流程**:
```
IKE_SA_INIT 完成 → 自动检测 ADDKE1 → IKE_INTERMEDIATE #1 (ML-KEM)
                → 自动检测 ADDKE2 → IKE_INTERMEDIATE #2 (SM2-KEM)
```

### 2.3 IKE_INTERMEDIATE #0 证书分发
**决策**: 利用消息 ID 判断（方案 A）

**实现**:
```c
// 在 ike_cert_post.c 的 should_send_intermediate_certs() 中
// 检查是否是第一个 IKE_INTERMEDIATE 消息 (message_id == 1)
if (message->get_message_id(message) != 1)
{
    return FALSE;
}
```

**原理**:
- IKE_SA_INIT 消息 ID = 0
- 第一个 IKE_INTERMEDIATE 消息 ID = 1（这就是 #0）
- 通过检查消息 ID 来触发证书分发

**注意事项**:
- 证书分发和 ADDKE1 可能在同一个 IKE_INTERMEDIATE 消息中
- 证书载荷 (CERT) 和密钥交换载荷 (KE) 是独立的

### 2.4 密钥派生
**决策**: 按照 RFC 9370 标准 PRF 链式派生

**派生公式**:
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

**IntAuth 生成** (用于 IKE_AUTH 认证):
```c
IntAuth = IntAuth_i | IntAuth_r | MID
```

### 2.5 IKE_AUTH 后量子认证
**决策**: 使用 ML-DSA/SLH-DSA 签名 IntAuth

**流程**:
1. 收集所有 IKE_INTERMEDIATE 交换数据
2. 生成 IntAuth = IntAuth_i | IntAuth_r | MID
3. 使用 AuthCert 的 ML-DSA 私钥签名 IntAuth
4. 发送 AUTH 载荷

**证书要求**:
- AuthCert 使用后量子签名算法 (ML-DSA-65 或 SLH-DSA-SHA2-128s)
- 当前 M3 模块已生成 SPHINCS+ 密钥对（原始格式）

---

## 3. 架构设计

### 3.1 整体架构
```
┌─────────────────────────────────────────────────────────────────┐
│                    PQ-GM-IKEv2 协议集成                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │   IKE_SA_INIT   │  │  ADDKE 处理    │  │ IKE_INTERMEDIATE│  │
│  │   (提议协商)    │  │  (RFC 9370)    │  │ (证书分发)      │  │
│  │                 │  │                 │  │                 │  │
│  │  KE=x25519      │  │  ADDKE1=ML-KEM │  │  #0: SignCert   │  │
│  │  ADDKE1=ML-KEM  │  │  ADDKE2=SM2    │  │      EncCert    │  │
│  │  ADDKE2=SM2     │  │                 │  │  #1: SM2-KEM   │  │
│  └─────────────────┘  └─────────────────┘  │  #2: ML-KEM    │  │
│          │                        │        └─────────────────┘  │
│          │                        │                        │    │
│          ▼                        ▼                        ▼    │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │   密钥派生      │  │   IntAuth      │  │   IKE_AUTH      │  │
│  │   (RFC 9370)    │  │   生成         │  │   (PQ认证)      │  │
│  │                 │  │                 │  │                 │  │
│  │  PRF 链式派生   │  │  收集中间交换  │  │  ML-DSA 签名    │  │
│  │                 │  │  数据          │  │  IntAuth        │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 数据流
```
Initiator                                    Responder
    |                                            |
    |--- IKE_SA_INIT --------------------------->|
    |     KE=x25519, ADDKE1=ml-kem, ADDKE2=sm2   |
    |                                            |
    |<-- IKE_SA_INIT ----------------------------|
    |     KE=x25519, ADDKE1=ml-kem, ADDKE2=sm2   |
    |                                            |
    |--- IKE_INTERMEDIATE #0 (mid=1) ----------->|
    |     CERT(SignCert), CERT(EncCert)          |
    |                                            |
    |<-- IKE_INTERMEDIATE #0 (mid=1) ------------|
    |     CERT(SignCert), CERT(EncCert)          |
    |                                            |
    |--- IKE_INTERMEDIATE #1 (mid=2) ----------->|
    |     KE(sm2-kem, 141 bytes)                 |
    |                                            |
    |<-- IKE_INTERMEDIATE #1 (mid=2) ------------|
    |     KE(sm2-kem, 141 bytes)                 |
    |     [共享密钥 SK_sm2 = r_i || r_r]         |
    |                                            |
    |--- IKE_INTERMEDIATE #2 (mid=3) ----------->|
    |     KE(ml-kem-768, 1184 bytes)             |
    |                                            |
    |<-- IKE_INTERMEDIATE #2 (mid=3) ------------|
    |     KE(ml-kem-768, 1184 bytes)             |
    |     [共享密钥 SK_mlkem]                    |
    |                                            |
    |--- IKE_AUTH (mid=4) ---------------------->|
    |     IDi, AUTH(ML-DSA签名 IntAuth)          |
    |                                            |
    |<-- IKE_AUTH (mid=4) -----------------------|
    |     IDr, AUTH(ML-DSA签名 IntAuth)          |
    |                                            |
    |--- IPsec SA 建立 ------------------------->|
```

### 3.3 代码修改点

#### 已完成的修改
| 文件 | 修改内容 | 状态 |
|------|----------|------|
| `ike_cert_post.c` | IKE_INTERMEDIATE #0 证书分发 | ✅ |
| `gmalg_ke.c/h` | SM2-KEM 实现 | ✅ |
| `gmalg_plugin.c` | SM2-KEM 注册 (KE_SM2=1051) | ✅ |
| `ml 插件` | ML-KEM-768 支持 | ✅ |

#### 需要新增/修改的文件
| 文件 | 修改内容 | 优先级 |
|------|----------|--------|
| `swanctl.conf` | 三重密钥交换配置 | 高 |
| `ike_cert_post.c` | 添加消息 ID 判断 | 高 |
| `test_pqgm_ikev2.sh` | 端到端测试脚本 | 中 |
| `keymat_v2.c` | 验证 RFC 9370 密钥派生 | 中 |
| `ike_auth.c` | IntAuth 收集 (已实现) | 低 |

---

## 4. 实现计划

### Phase 1: 配置和验证 (高优先级)
1. 创建三重密钥交换的 swanctl.conf 配置
2. 修改 ike_cert_post.c 添加消息 ID 判断
3. 重新编译 strongSwan
4. 验证 IKE_SA_INIT 提案协商

### Phase 2: 流程测试 (中优先级)
1. 测试 IKE_INTERMEDIATE #0 证书分发
2. 测试 ADDKE1 (ML-KEM) 执行
3. 测试 ADDKE2 (SM2-KEM) 执行
4. 验证密钥派生链

### Phase 3: 认证集成 (低优先级)
1. 验证 IntAuth 生成
2. 集成 ML-DSA 认证
3. 端到端连通性测试

---

## 5. 风险和缓解

### 5.1 技术风险
| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| ADDKE 执行顺序不确定 | 协议失败 | 查看 strongSwan 源码确认顺序 |
| 消息 ID 判断不准确 | 证书分发失败 | 添加详细日志，逐步调试 |
| ML-DSA 证书格式不支持 | 认证失败 | 使用 SPHINCS+ 或跳过认证 |

### 5.2 时间风险
| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 配置调试耗时 | 延期 | 使用现有测试案例作为参考 |
| strongSwan 内核问题 | 阻塞 | 准备降级方案 |

---

## 6. 验收标准

### 6.1 功能验收
- [ ] IKE_SA_INIT 成功协商三种密钥交换
- [ ] IKE_INTERMEDIATE #0 成功交换双证书
- [ ] ADDKE1 (ML-KEM) 成功执行
- [ ] ADDKE2 (SM2-KEM) 成功执行
- [ ] 密钥派生链正确
- [ ] IPsec SA 成功建立
- [ ] 双向流量通信正常

### 6.2 性能验收
- [ ] 记录完整协议流程时延
- [ ] 对比传统 IKEv2 性能差异
- [ ] 收集论文所需数据

---

## 7. 参考资料

- RFC 7296: IKEv2
- RFC 9242: IKE_INTERMEDIATE
- RFC 9370: Multiple Key Exchanges
- FIPS 203: ML-KEM
- FIPS 204: ML-DSA
- GM/T 0002-0004-2012: SM2/SM3/SM4
- strongSwan 6.0 源码
