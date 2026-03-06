# PQ-GM-IKEv2 5-RTT 国密对称栈测试摘要

**测试时间**: 2026-03-05 17:21

**测试配置**: pqgm-5rtt-gm-symm

**测试结果**: ✅ 成功

## 测试环境

- **Initiator IP**: 192.168.172.134
- **Responder IP**: 192.168.172.132
- **strongSwan版本**: 6.0.4
- **算法**:
  - IKE: SM4-CBC + HMAC-SM3-128 + PRF-SM3
  - KE: x25519
  - ADDKE1: SM2-KEM
  - ADDKE2: ML-KEM-768
  - Auth: ML-DSA-65
  - ESP: AES_GCM_16-256 (内核限制)

## 5-RTT 协议流程

所有阶段与标准测试相同，区别仅在于IKE SA的加密和PRF算法。

### IKE提案对比

| 配置 | 加密 | 完整性 | PRF |
|------|------|--------|-----|
| pqgm-5rtt-mldsa | AES-256-CBC | HMAC-SHA256-128 | PRF-SHA256 |
| pqgm-5rtt-gm-symm | SM4-CBC | HMAC-SM3-128 | PRF-SM3 |

### CHILD_SA 状态

**IKE_SA**: ESTABLISHED ✅
**CHILD_SA**: INSTALLED ✅

**隧道信息**:
- 本地子网: 10.2.0.0/16
- 远端子网: 10.1.0.0/16
- **ESP算法**: AES_GCM_16-256 ⚠️

## 关键发现

⚠️ **ESP算法限制**: 
- **预期**: SM4-CBC + HMAC-SM3
- **实际**: AES_GCM_16-256
- **原因**: Linux内核IPsec不支持SM4-CBC ESP算法
- **影响**: 数据平面加密使用AES-GCM， IKE SA仍使用SM4/HMAC-SM3

## 两种配置对比

### 共同点
1. ✅ 三重密钥交换（x25519 + SM2-KEM + ML-KEM-768）
2. ✅ 双证书机制（SM2签名+加密）
3. ✅ 后量子签名认证（ML-DSA-65）
4. ✅ 5-RTT协议流程

### 不同点
1. **IKE SA加密/PRF**:
   - 标准: AES-256-CBC + HMAC-SHA256 + PRF-SHA256
   - 国密: SM4-CBC + HMAC-SM3 + PRF-SM3

2. **ESP算法**:
   - 两者都使用: AES_GCM_16-256（内核限制）

---
*测试完成时间: 2026-03-05 17:21*
