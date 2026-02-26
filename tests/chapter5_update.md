# 第五章 系统实现与性能评估

## 5.1 实验环境搭建

### 5.1.1 硬件环境

| 设备 | 配置 |
|------|------|
| 虚拟机平台 | VMware Workstation |
| 操作系统 | Ubuntu 22.04 LTS (Linux 6.8.0-101-generic) |
| CPU | x86_64 |
| 内存 | 4 GB |
| 网络 | NAT 模式 |

### 5.1.2 软件环境

| 软件 | 版本 | 说明 |
|------|------|------|
| strongSwan | 6.0.4 | 支持 ML-KEM 插件 |
| OpenSSL | 3.0.2 | 提供 ML-KEM 算法支持 |
| ML-KEM Plugin | 1.0 | 后量子密钥封装机制 |

### 5.1.3 网络拓扑

```
┌─────────────────────────┐         ┌─────────────────────────┐
│     Initiator VM        │         │     Responder VM        │
│   192.168.172.130       │         │   192.168.172.131       │
│                         │         │                         │
│  - swanctl              │◄───────►│  - swanctl              │
│  - charon (IKE daemon)  │  IKEv2  │  - charon (IKE daemon)  │
│  - ML-KEM plugin        │         │  - ML-KEM plugin        │
└─────────────────────────┘         └─────────────────────────┘
```

---

## 5.2 系统实现

### 5.2.1 配置文件设计

strongSwan 使用 swanctl 配置工具，配置文件位于 `/etc/swanctl/swanctl.conf`。

**基线连接配置（传统 IKEv2，x25519）**：

```conf
connections {
    baseline {
        remote_addrs = 192.168.172.131
        local {
            auth = pubkey
            certs = initiatorCert.pem
            id = "C=CN, O=PQGM-Test, CN=initiator.pqgm.test"
        }
        remote {
            auth = pubkey
            id = "C=CN, O=PQGM-Test, CN=responder.pqgm.test"
        }
        children {
            net {
                local_ts = 10.1.1.0/24
                remote_ts = 10.1.2.0/24
                esp_proposals = aes256-sha256-x25519
                start_action = start
            }
        }
        version = 2
        proposals = aes256-sha256-x25519
    }
}
```

**混合密钥交换配置（x25519 + ML-KEM-768）**：

```conf
connections {
    pqgm-hybrid {
        remote_addrs = 192.168.172.131
        local {
            auth = pubkey
            certs = initiatorCert.pem
            id = "C=CN, O=PQGM-Test, CN=initiator.pqgm.test"
        }
        remote {
            auth = pubkey
            id = "C=CN, O=PQGM-Test, CN=responder.pqgm.test"
        }
        children {
            net {
                local_ts = 10.1.1.0/24
                remote_ts = 10.1.2.0/24
                esp_proposals = aes256gcm16-sha256-x25519-ke1_mlkem768
                start_action = start
            }
        }
        version = 2
        proposals = aes256-sha256-x25519-ke1_mlkem768
    }
}
```

### 5.2.2 握手流程分析

**传统 IKEv2 握手（2-RTT）**：

```
Initiator                                    Responder
    │                                           │
    │────────── IKE_SA_INIT (Request) ──────────│
    │     (SA, KE=x25519, Nonce)                │
    │                                           │
    │───────── IKE_SA_INIT (Response) ─────────│
    │     (SA, KE=x25519, Nonce, CERTREQ)       │
    │                                           │
    │─────────── IKE_AUTH (Request) ────────────│
    │     (IDi, CERT, AUTH, SA, TSi, TSr)       │
    │                                           │
    │────────── IKE_AUTH (Response) ────────────│
    │     (IDr, CERT, AUTH, SA, TSi, TSr)       │
    │                                           │
    │           ▼ 连接建立完成 ▼                │
```

**混合密钥交换握手（3-RTT，RFC 9370）**：

```
Initiator                                    Responder
    │                                           │
    │────────── IKE_SA_INIT (Request) ──────────│
    │     (SA, KE=x25519, Nonce, KE1=ML-KEM)    │
    │                                           │
    │───────── IKE_SA_INIT (Response) ─────────│
    │     (SA, KE=x25519, Nonce, KE1=ML-KEM,    │
    │      CERTREQ)                             │
    │                                           │
    │─────── IKE_INTERMEDIATE (Request) ───────│
    │     (KE2=ML-KEM-768 Ciphertext)           │
    │                                           │
    │───── IKE_INTERMEDIATE (Response) ────────│
    │     (KE2=ML-KEM-768 Ciphertext)           │
    │                                           │
    │─────────── IKE_AUTH (Request) ────────────│
    │     (IDi, CERT, AUTH, SA, TSi, TSr)       │
    │                                           │
    │────────── IKE_AUTH (Response) ────────────│
    │     (IDr, CERT, AUTH, SA, TSi, TSr)       │
    │                                           │
    │           ▼ 连接建立完成 ▼                │
```

---

## 5.3 性能评估

### 5.3.1 测试方法

1. **测试次数**：每种配置进行 10 次独立测试
2. **测试指标**：
   - 握手时延（从发起请求到连接建立的时间）
   - 通信开销（各阶段报文大小）
   - 连接成功率

3. **测试脚本**：使用自动化测试脚本 `measure_latency.sh`

### 5.3.2 握手时延测试

**表 5-1 握手时延测试结果**

| 配置 | 密钥交换方法 | RTT | 平均时延 | 最小时延 | 最大时延 | 成功率 |
|------|-------------|-----|----------|----------|----------|--------|
| 传统 IKEv2 | x25519 | 2 | **48 ms** | 42 ms | 52 ms | 100% |
| 混合密钥交换 | x25519 + ML-KEM-768 | 3 | **52 ms** | 50 ms | 56 ms | 100% |

**分析**：
- 混合密钥交换增加约 **4 ms (8.3%)** 的握手时延
- 时延增加主要来自额外的 IKE_INTERMEDIATE 交换
- 在虚拟机局域网环境中，额外时延较小

**原始数据**：

传统 IKEv2（10次测试，单位：ms）：
```
46, 50, 49, 46, 48, 52, 50, 50, 42, 49
平均值: 48 ms
标准差: 2.9 ms
```

混合密钥交换（10次测试，单位：ms）：
```
53, 50, 53, 52, 53, 51, 50, 56, 51, 51
平均值: 52 ms
标准差: 1.8 ms
```

### 5.3.3 通信开销测试

**表 5-2 各阶段报文大小对比**

| 报文类型 | 基线 (bytes) | 混合 (bytes) | 增量 |
|----------|-------------|-------------|------|
| IKE_SA_INIT 请求 | 240 | 284 | +44 |
| IKE_SA_INIT 响应 | 273 | 317 | +44 |
| IKE_INTERMEDIATE 请求 | N/A | 1268 | +1268 |
| IKE_INTERMEDIATE 响应 | N/A | 1200 | +1200 |
| IKE_AUTH 请求 | 832 | 832 | 0 |
| IKE_AUTH 响应 | 704 | 704 | 0 |
| **总计** | **2049** | **4605** | **+2556** |

**分析**：
- 混合密钥交换增加约 **125%** 的通信开销
- 主要开销来自 ML-KEM-768 密文传输（约 1184 字节）
- IKE_AUTH 阶段无额外开销

**报文增量详解**：

1. **IKE_SA_INIT 阶段增量（+44 bytes）**：
   - 来自 SA 提议中增加的 ML-KEM-768 变换参数
   - Transform 长度增加 8 字节

2. **IKE_INTERMEDIATE 阶段（+2468 bytes）**：
   - ML-KEM-768 公钥封装密文：1184 字节
   - IKEv2 消息头和载荷封装开销：约 80 字节
   - 两个方向各传输一次密文

### 5.3.4 性能权衡分析

| 指标 | 基线 | 混合 | 变化 |
|------|------|------|------|
| 握手时延 | 48 ms | 52 ms | +8.3% |
| 通信开销 | 2049 bytes | 4605 bytes | +125% |
| 抗量子安全性 | 无 | 有（NIST Level 3） | ∞ |

**结论**：
1. **时延增加可接受**：仅增加 4 ms，对用户体验影响较小
2. **通信开销较大**：增加 125%，需要考虑带宽限制场景
3. **安全性提升显著**：获得抗量子攻击的保护

---

## 5.4 相关工作对比

**表 5-3 与其他方案的对比**

| 方案 | 密钥交换 | 时延增加 | 通信开销 | 抗量子安全性 |
|------|---------|---------|---------|-------------|
| 传统 IKEv2 | ECDH (x25519) | - | - | 无 |
| Kyber in IKEv2 | Kyber-768 + ECDH | +12% | +130% | 有 |
| 本文方案 | ML-KEM-768 + x25519 | +8.3% | +125% | 有 |

---

## 5.5 本章小结

本章在 strongSwan 6.0.4 基础上实现了 ML-KEM 混合密钥交换方案，并进行了性能评估。实验结果表明：

1. **实现可行性**：strongSwan 原生支持 ML-KEM，无需修改源码即可实现混合密钥交换
2. **性能可接受**：握手时延仅增加 8.3%，对实际部署影响较小
3. **通信开销**：ML-KEM-768 密文较大，通信开销增加 125%
4. **安全增强**：混合方案提供抗量子安全性，同时保持与经典算法的兼容性

下一步工作将包括：
1. 集成 SM2/SM3/SM4 国密算法
2. 实现 SM2-KEM 密钥交换
3. 实现 Early Gating DoS 防护机制
4. 在真实网络环境中进行更大规模测试
