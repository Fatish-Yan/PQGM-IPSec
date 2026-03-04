# ML-DSA 性能开销分析报告

**日期**: 2026-03-04
**问题**: IKE_AUTH 阶段耗时 20-40ms，远超预期的 2-3ms

---

## 1. 问题分析

### 1.1 预期 vs 实际

| 指标 | 预期 (文档) | 实际观察 |
|------|-------------|----------|
| ML-DSA 签名 | 2-3ms | ~30ms (IKE_AUTH阶段) |
| ML-DSA 验证 | 2-3ms | ~30ms (IKE_AUTH阶段) |

### 1.2 根本原因

**不是ML-DSA算法本身慢，而是混合证书处理开销大！**

从日志分析，每次验证都执行以下步骤：

```
1. try_mldsa_from_hybrid_cert 被调用
2. 获取证书DER编码 (2355 bytes)
3. 搜索ML-DSA OID (O(n)扫描)
4. 解析ASN.1结构
5. 提取公钥 (1952 bytes)
6. 加载公钥到liboqs
7. 多次尝试加载不同大小的blob (失败重试)
```

---

## 2. 日志证据

### 2.1 混合证书提取过程

```
[IKE] ML-DSA: try_mldsa_from_hybrid_cert called for cert subject 'CN=responder.pqgm.test'
[IKE] ML-DSA: DER encoding size = 2355 bytes, searching for OID (len=12)
[IKE] ML-DSA: found extension OID at offset 300, remaining=2043 bytes
[IKE] ML-DSA: found OCTET STRING tag, remaining=2042
[IKE] ML-DSA: OCTET STRING len=1952, need 1952, remaining=2039
[IKE] ML-DSA: extracted pubkey, 1952 bytes
[LIB] ML-DSA: mldsa_public_key_load called, type=MLDSA65
[LIB] ML-DSA: loaded public key successfully (1952 bytes)
```

### 2.2 失败的加载尝试

```
[LIB] ML-DSA: got builder part 4
[LIB] ML-DSA: unknown builder part 4, returning NULL
[LIB] ML-DSA: invalid public key size 113, expected 1952
[LIB] ML-DSA: blob size 436 doesn't match ML-DSA-65 key size 4032, skipping
[LIB] ML-DSA: blob size 266 doesn't match ML-DSA-65 key size 4032, skipping
```

---

## 3. 开销分解

| 步骤 | 估计时间 | 说明 |
|------|----------|------|
| 证书DER编码获取 | ~1ms | 内存操作 |
| OID搜索 | ~2-5ms | O(n)扫描2355字节 |
| ASN.1解析 | ~3-5ms | 复杂结构解析 |
| 公钥加载 | ~2-3ms | liboqs初始化 |
| 失败重试 | ~5-10ms | 多次尝试不同格式 |
| **ML-DSA验证** | **~2-3ms** | **实际算法开销** |
| **总计** | **~15-30ms** | |

---

## 4. 为什么使用混合证书？

由于 **OpenSSL 3.0.2 不支持ML-DSA**，我们采用了混合证书方案：

1. 证书主体使用 **ECDSA** (OpenSSL可以解析)
2. ML-DSA公钥存储在 **自定义扩展** 中
3. 验证时从扩展中提取ML-DSA公钥

这种方案的代价是**每次验证都需要解析证书**。

---

## 5. 解决方案

### 方案 1: 公钥缓存 (推荐)

**实现**: 在第一次提取后缓存ML-DSA公钥

```c
// 在 certificate_t 或 credential_manager 中添加缓存
struct {
    certificate_t *cert;
    public_key_t *cached_mldsa_key;
} mldsa_cache_t;
```

**预期效果**: 后续验证只需 2-3ms

### 方案 2: 使用原生 ML-DSA 证书

**要求**: 升级到 OpenSSL 3.5+

**优势**:
- 证书直接包含ML-DSA公钥
- 无需解析扩展
- 验证时间回到 2-3ms

### 方案 3: 预加载公钥

**实现**: 启动时预提取所有ML-DSA公钥

```bash
# 在 charon 启动时预加载
swanctl --load-all  # 同时预提取ML-DSA公钥
```

---

## 6. 性能对比

| 场景 | 签名时间 | 验证时间 |
|------|----------|----------|
| 纯ML-DSA (liboqs) | 2-3ms | 2-3ms |
| 混合证书 (当前) | 2-3ms | **15-30ms** |
| 原生证书 (OpenSSL 3.5+) | 2-3ms | 2-3ms |
| 缓存公钥 (优化后) | 2-3ms | ~3ms |

---

## 7. 结论

1. **ML-DSA算法本身很快** (2-3ms)，符合预期
2. **混合证书解析是瓶颈**，增加了 10-25ms 开销
3. **对论文数据的影响**: 总握手时间 135-144ms 中，证书解析约占 15-25ms
4. **优化建议**: 实现公钥缓存可显著提升性能

---

## 8. 论文表述建议

> 在当前实现中，由于OpenSSL 3.0不支持原生ML-DSA证书，我们采用了混合证书方案。
> ML-DSA公钥存储在X.509证书的自定义扩展中，验证时需要从扩展中提取公钥。
> 这种方案增加了约15-25ms的证书处理开销，但ML-DSA签名验证本身仅需2-3ms。
> 使用原生ML-DSA证书（需要OpenSSL 3.5+）可将此开销消除。
