# 数据包统计报告

## 总体统计

- **总数据包数**: 20个（10个接收，10个发送）
- **测试时长**: 约2秒

## 各阶段数据包统计

### RTT 1: IKE_SA_INIT
- **接收**: 1个数据包
  - IKE_SA_INIT request（264字节）
- **发送**: 1个数据包
  - IKE_SA_INIT response（337字节）

### RTT 2: IKE_INTERMEDIATE #0 (证书分发)
- **接收**: 1个数据包
  - IKE_INTERMEDIATE request 1（912字节）
- **发送**: 1个数据包
  - IKE_INTERMEDIATE response 1（包含双证书）

### RTT 3: IKE_INTERMEDIATE #1 (SM2-KEM)
- **接收**: 1个数据包
  - IKE_INTERMEDIATE request 2（包含SM2-KEM载荷）
- **发送**: 1个数据包
  - IKE_INTERMEDIATE response 2（包含SM2-KEM响应）

### RTT 4: IKE_INTERMEDIATE #2 (ML-KEM-768)
- **接收**: 3个数据包
  - IKE_INTERMEDIATE request 3 EF(1/2)（第1个分片）
  - IKE_INTERMEDIATE request 3 EF(2/2)（第2个分片）
  - IKE_INTERMEDIATE request 3 KE（密钥交换载荷）
- **发送**: 1个数据包
  - IKE_INTERMEDIATE response 3（包含ML-KEM响应）

### RTT 5: IKE_AUTH (ML-DSA认证)
- **接收**: 7个数据包
  - IKE_AUTH request 4 EF(1/6) - EF(6/6)（6个分片）
  - IKE_AUTH request 4 完整载荷（IDi, CERT, AUTH等）
- **发送**: 1个数据包
  - IKE_AUTH response 4（认证成功响应）

## 密钥交换详情

### RFC 9370 多重密钥派生

1. **Initial Key Derivation** (IKE_SA_INIT后)
   - SKEYSEED = prf(Ni | Nr | DH)
   - 派生: SK_d, SK_ai, SK_ar, SK_pi, SK_pr

2. **Update after SM2-KEM** (IKE_INTERMEDIATE #1后)
   - SKEYSEED = prf(SK_d(prev) | SM2-KEM shared secret)
   - 更新: SK_d, SK_pi, SK_pr

3. **Update after ML-KEM** (IKE_INTERMEDIATE #2后)
   - SKEYSEED = prf(SK_d(prev) | ML-KEM shared secret)
   - 最终密钥: SK_d, SK_ai, SK_ar, SK_pi, SK_pr

## 证书使用

### Initiator证书
- **SM2签名证书**: signCert.pem（575字节DER）
- **SM2加密证书**: encCert.pem（575字节DER）
- **ML-DSA认证证书**: initiator_hybrid_cert.pem

### Responder证书
- **SM2签名证书**: signCert.pem（834字节PEM）
- **SM2加密证书**: encCert.pem（834字节PEM）
- **ML-DSA认证证书**: responder_hybrid_cert.pem

---
*生成时间: 2026-03-05 17:18*
