# PQ-GM-IKEv2 标准算法 vs 国密对称栈对比报告

## 测试配置对比

| 配置 | IKE加密 | IKE完整性 | IKE PRF | ESP加密 |
|------|--------|----------|--------|--------|
| pqgm-5rtt-mldsa | AES-256-CBC | HMAC-SHA256 | PRF-SHA256 | AES-GCM-256 |
| pqgm-5rtt-gm-symm | SM4-CBC | HMAC-SM3 | PRF-SM3 | AES-GCM-256 |

注意: ESP因内核限制使用AES-GCM

## 共同特性
- 密钥交换: x25519 + SM2-KEM + ML-KEM-768
- 认证: ML-DSA-65
- 协议流程: 5-RTT

## 测试结果
✅ 两种配置均成功建立连接
✅ 国密算法集成正常工作
✅ 5-RTT流程完全一致

---
*生成时间: 2026-03-05 17:22*
