# GM 对称栈性能测试数据

**测试日期**: 2026-03-04
**测试环境**: Docker 容器 (Ubuntu 22.04)
**网络**: 虚拟网桥 docker_pqgm_net (172.28.0.0/24)

---

## 1. 测试配置

### IKE 提案
```
SM4_CBC_128/HMAC_SM3_128/PRF_SM3/CURVE_25519/KE1_KE_SM2/KE2_ML_KEM_768
```

### ESP 提案
```
AES_GCM_16_256 (临时替代，内核不支持SM4-CBC)
```

### 密钥交换
- KE0: X25519 (经典 DH)
- KE1: SM2-KEM (国密)
- KE2: ML-KEM-768 (后量子)

### 认证
- ML-DSA-65 (后量子签名)

---

## 2. 5-RTT 握手时序

从 PCAP 文件提取的时间戳：

| RTT | 消息类型 | 发送时间 | 接收时间 | 延迟 |
|-----|----------|----------|----------|------|
| 1 | IKE_SA_INIT [I] | 19:13:56.880287 | 19:13:56.886396 | ~6ms |
| 2 | IKE_INTERMEDIATE #0 [I] | 19:13:56.891112 | 19:13:56.893025 | ~2ms |
| 3 | IKE_INTERMEDIATE #1 [I] | 19:13:56.895096 | 19:13:56.897587 | ~2ms |
| 4 | IKE_INTERMEDIATE #2 [I] | 19:13:56.904408 | 19:13:56.907898 | ~3ms |
| 5 | IKE_AUTH [I] (6 fragments) | 19:13:56.911147 | 19:13:56.940xxx | ~30ms |

**总握手时间**: ~60-70ms (initiator 日志显示 152ms)

---

## 3. 消息大小统计

| 消息 | 方向 | 大小 |
|------|------|------|
| IKE_SA_INIT Request | I→R | 264 bytes |
| IKE_SA_INIT Response | R→I | 317 bytes |
| IKE_INTERMEDIATE #0 Request | I→R | 1232 bytes |
| IKE_INTERMEDIATE #0 Response | R→I | 1232 bytes |
| IKE_INTERMEDIATE #1 Request | I→R | 224 bytes |
| IKE_INTERMEDIATE #1 Response | R→I | 224 bytes |
| IKE_INTERMEDIATE #2 Request | I→R | 1236 + 100 bytes (2 fragments) |
| IKE_INTERMEDIATE #2 Response | R→I | 1168 bytes |
| IKE_AUTH Request | I→R | 6 x 1236/164 bytes (6 fragments) |
| IKE_AUTH Response | R→I | 5 x 1236 bytes (5 fragments) |

**总数据传输**: ~15KB (双向)

---

## 4. 算法性能 (从日志提取)

### SM3 Hash
- 输出大小: 32 bytes (256 bits)
- 增量模式: 支持

### SM4-CBC 加密
- 块大小: 16 bytes (128 bits)
- 密钥大小: 16 bytes (128 bits)
- 性能: ~175 MB/s (基准测试)

### HMAC-SM3
- 输出大小: 32 bytes
- 截断版本: 16 bytes (HMAC-SM3-128)
- 增量模式: 支持

### SM2-KEM
- 密文大小: 139 bytes
- 共享密钥: 64 bytes

### ML-DSA-65 签名
- 签名大小: 3309 bytes
- 公钥大小: 1952 bytes

---

## 5. 测试结果

```
[IKE] IKE_SA pqgm-5rtt-gm-symm[2] established!
[IKE] CHILD_SA net{2} established with SPIs c4e24a99_i cb001041_o
initiate completed successfully
```

**状态**: ✅ 成功

---

## 6. 文件清单

| 文件 | 说明 |
|------|------|
| gm-symmetric-stack.pcap | 网络抓包文件 (79KB) |
| gm-symm-full-test.log | 完整测试日志 (24KB) |
| performance-summary.md | 本文档 |

---

## 7. 注意事项

1. **ESP层限制**: Linux内核不支持 `cbc(sm4)`，ESP临时使用AES-GCM-256
2. **IKE层国密**: SM4-CBC + HMAC-SM3-128 + PRF-SM3 完全工作
3. **证书**: SM2双证书(签名+加密) + ML-DSA混合证书
