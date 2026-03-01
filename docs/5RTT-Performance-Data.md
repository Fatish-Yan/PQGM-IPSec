# 5-RTT PQ-GM-IKEv2 性能测试报告

## 测试环境

| 参数 | 值 |
|------|-----|
| 测试日期 | 2026-03-01 |
| 测试平台 | Docker (Ubuntu 22.04) |
| 发起端地址 | 172.28.0.10 |
| 响应端地址 | 172.28.0.20 |
| strongSwan 版本 | 6.0.4 (修改版) |
| 认证方式 | PSK (Pre-Shared Key) |

## 协议配置

| 密钥交换算法 | 说明 |
|-------------|------|
| x25519 | 经典椭圆曲线 Diffie-Hellman |
| SM2-KEM | 国密密钥封装机制 |
| ML-KEM-768 | 后量子密钥封装机制 (NIST FIPS 203) |

## 测试结果

### 总体性能

| 指标 | 值 |
|------|-----|
| 总握手时间 | 115 ms |
| 总交换包数 | 11 个 |
| IKE_SA 状态 | ✅ 建立成功 |
| CHILD_SA 状态 | ✅ 建立成功 |

### 5-RTT 详细分析

| RTT | 阶段 | 功能 | 请求包大小 | 响应包大小 | 处理时间 |
|-----|------|------|-----------|-----------|---------|
| 1 | IKE_SA_INIT | x25519 KE 协商 | 264 bytes | 297 bytes | ~0.5 ms |
| 2 | IKE_INTERMEDIATE #0 | SM2 双证书交换 | 864 bytes | 864 bytes | ~0.6 ms |
| 3 | IKE_INTERMEDIATE #1 | SM2-KEM 密钥交换 | 224 bytes | 224 bytes | ~34 ms |
| 4 | IKE_INTERMEDIATE #2 | ML-KEM-768 密钥交换 | 1336 bytes | 1168 bytes | ~32 ms |
| 5 | IKE_AUTH | PSK 认证 | 320 bytes | 240 bytes | ~1 ms |

### 数据包统计

```\n
消息类型统计:
- IKE_SA_INIT: 2 packets (1 request + 1 response)
- IKE_INTERMEDIATE: 6 packets (3 requests + 3 responses)
- IKE_AUTH: 2 packets (1 request + 1 response)
- IKE_INTERMEDIATE 分片: 2 fragments (ML-KEM-768)

总数据传输:
- 发起端发送: ~3008 bytes
- 响应端发送: ~2793 bytes
- 总计: ~5801 bytes
```\n

### 密钥交换验证

| 密钥交换 | 共享密钥验证 | 状态 |
|----------|-------------|------|
| x25519 | 双方计算出相同的 DH 共享密钥 | ✅ |
| SM2-KEM | SK = r_i \|\| r_r (64 bytes) | ✅ 匹配 |
| ML-KEM-768 | 双方计算出相同的 KEM 共享密钥 | ✅ |

### IKE_SA 信息

```
IKE_SA: pqgm-ikev2
Local:  172.28.0.10[initiator.pqgm.test]
Remote: 172.28.0.20[responder.pqgm.test]
Auth:   PSK
Rekey:  ~13000s
```

### CHILD_SA 信息

```
CHILD_SA: ipsec
SPIs:    c6b637bc_i / c549b879_o
TS:      10.1.0.0/16 === 10.2.0.0/16
ESP:     AES_GCM_16_256
```

## 与传统 IKEv2 对比

| 协议 | RTT 数 | 额外密钥交换 | 量子安全 |
|------|--------|-------------|---------|
| 标准 IKEv2 (1-RTT) | 1 | 无 | ❌ |
| RFC 9370 (3-RTT) | 3 | ML-KEM | ✅ |
| PQ-GM-IKEv2 (5-RTT) | 5 | SM2-KEM + ML-KEM | ✅ |

## 安全特性

1. **经典安全**: x25519 (128-bit 安全强度)
2. **国密支持**: SM2-KEM (256-bit SM2 曲线)
3. **后量子安全**: ML-KEM-768 (NIST Level 3)
4. **混合保护**: 任意一个密钥交换安全即可保证整体安全

## 抓包文件

```
/home/ipsec/PQGM-IPSec/captures/5rtt_complete_20260301_165103.pcap
```

可使用 Wireshark 打开分析。

## 结论

5-RTT PQ-GM-IKEv2 协议在 Docker 测试环境中成功运行：
- ✅ 完整的 5 个往返通信
- ✅ SM2 双证书交换
- ✅ SM2-KEM 密钥交换
- ✅ ML-KEM-768 密钥交换
- ✅ PSK 认证成功
- ✅ IKE_SA 和 CHILD_SA 建立成功
- ✅ 总握手时间约 115ms

