# 论文草稿审查报告

> 审查时间: 2026-02-27
> 审查对象: 《第五章 系统实现与测试草稿》
> 审查目的: 识别草稿中与实际项目实现不符的问题

---

## 🔴 严重错误（必须修改）

### 1. Transform ID 错误

**草稿原文（多处出现）**:
> "私有协商 ID 60001"
> "包含私有 ID 60001 和 ML-KEM ID 35 的 SA 提案"
> "承载基于 ID 60001 的 SM2 数字信封交互"

**实际实现**:
```c
// gmalg_plugin.h
#define KE_SM2  1051  // SM2 Key Exchange
```

**问题**: SM2-KEM 的 Transform ID 应该是 **1051**，不是 60001

**修正建议**: 全文搜索 "60001"，替换为 "1051"

---

### 2. 插件名称错误

**草稿原文**:
> "新增开发 gmssl_plugin 模块"
> "调用 gmssl_plugin 校验基于 SM2 的证书链属性"

**实际实现**:
```
strongswan/src/libstrongswan/plugins/gmalg/
├── gmalg_plugin.c
├── gmalg_hasher.c
├── gmalg_crypter.c
├── gmalg_signer.c
└── gmalg_ke.c
```

**问题**: 插件名称是 **gmalg**（国密算法插件），不是 gmssl_plugin

**修正建议**: 全文搜索 "gmssl_plugin"，替换为 "gmalg 插件" 或 "gmalg_plugin"

---

### 3. SM4 加密模式错误

**草稿原文**:
> "实现 aead_t 接口封装 sm4_gcm_encrypt()，使其支持 IPsec ESP 载荷的处理"

**实际实现**:
```c
// gmalg_crypter.c 支持的模式
typedef enum {
    SM4_MODE_ECB = 0,
    SM4_MODE_CBC = 1,
    SM4_MODE_CTR = 2,
} sm4_mode_t;
```

**问题**:
1. 实际实现的是 **crypter_t** 接口，不是 aead_t
2. 支持的是 **ECB/CBC/CTR** 模式，**没有 GCM 模式**
3. 项目目前没有实现 ESP 载荷处理

**修正建议**: 改为 "实现 crypter_t 接口封装 SM4 ECB/CBC/CTR 模式"

---

### 4. 后量子密码库错误

**草稿原文**:
> "后量子密码库: liboqs (Open Quantum Safe项目，支持 ML-KEM)"
> "并通过 strongSwan 自带的 oqs 插件接入"
> "直接编译并启用 strongSwan 源码树中的 oqs 插件"

**实际实现**:
```bash
$ ls /home/ipsec/strongswan/src/libstrongswan/plugins/ | grep -E "oqs|ml"
ml
```

**问题**:
1. 项目使用的是 strongSwan 内置的 **ml 插件**，不是 oqs 插件
2. 没有使用 liboqs 库
3. ml 插件是 strongSwan 原生实现，不依赖外部 liboqs

**修正建议**: 将 "liboqs" 和 "oqs 插件" 替换为 "strongSwan 内置 ml 插件"

---

### 5. 性能数据编造

**草稿原文（表 5-3）**:
| 测试场景 | 握手轮次 | 平均时延 (ms) |
| :--- | :--- | :--- |
| 传统 IKEv2 (Baseline) | 2-RTT | 21.5 |
| PQ-GM-IKEv2 (混合模式) | 5-RTT | 98.4 |

**实际测试结果**:
| 配置 | 密钥交换方法 | RTT | 平均时延 |
|------|-------------|-----|----------|
| 传统 IKEv2 | x25519 | 2 | **48 ms** |
| 混合密钥交换 | x25519 + ML-KEM-768 | 3 | **52 ms** |

**问题**:
1. 传统 IKEv2 时延 21.5ms 是**编造的**，实际测试约 48ms
2. PQ-GM-IKEv2 时延 98.4ms 是**编造的**，尚未完成 5-RTT 完整测试
3. 早期门控验证 34.2ms 也是**编造的**

**修正建议**:
1. 使用实际测试数据（48ms / 52ms）
2. 或标注 "待测试" 等完成 5-RTT 测试后补充

---

## 🟡 中等问题（建议修改）

### 6. strongSwan 版本不准确

**草稿原文**:
> "strongSwan 6.0.0 (支持 RFC 9370 多重密钥交换扩展)"

**实际环境**:
- strongSwan **6.0.4**

**修正建议**: 将 "6.0.0" 改为 "6.0.4"

---

### 7. 内核版本不准确

**草稿原文**:
> "Ubuntu 22.04 LTS (Kernel 5.15)"

**实际环境**:
```bash
$ uname -r
6.8.0-101-generic
```

**修正建议**: 将 "Kernel 5.15" 改为 "Kernel 6.8.0"

---

### 8. CPU 信息待确认

**草稿原文**:
> "CPU / 内存: Intel Core i7-12700K (分配 4 核心) / 8GB RAM"

**问题**: 需要确认 VMware 虚拟机的实际配置

**修正建议**: 运行 `lscpu` 和 `free -h` 获取实际配置

---

### 9. 通信开销数据编造

**草稿原文（表 5-2）**:
| 交互阶段 | 传统 IKEv2 | PQ-GM-IKEv2 |
| :--- | :--- | :--- |
| IKE_SA_INIT | ~ 450 | ~ 520 |
| R0 (双证书分发) | N/A | ~ 2450 |
| R1 (SM2 数字信封) | N/A | ~ 280 |
| R2 (ML-KEM 交换) | N/A | ~ 2580 |
| 总交互流量 | ~ 1700 | ~ 6380 |

**实际数据**:
- SM2-KEM 密文: **141 字节**（不是 280 字节）
- ML-KEM-768 密文: **1184 字节**
- SM2 证书大小: ~834 字节/张

**问题**: 这些数据是编造的，与实际实现不符

**修正建议**: 通过 Wireshark 实际抓包测量，或使用以下参考值：
- SM2 证书: ~800-900 字节/张
- SM2-KEM 密文: ~141 字节
- ML-KEM-768 密文: 1184 字节

---

## 🟢 轻微问题（可选修改）

### 10. 签名认证描述不准确

**草稿原文**:
> "强制重定向上下文到 oqs 插件，调用 ML-DSA-65 签名算法"

**实际实现**:
- 目前 IKE_AUTH 阶段使用的是 **SM2 签名**
- 后量子签名认证（ML-DSA/SLH-DSA）**尚未实现**
- SPHINCS+ 密钥已生成，但尚未集成到认证流程

**修正建议**: 说明当前使用 SM2 签名，后量子认证是未来工作

---

### 11. 日志输出编造

**草稿原文**:
```
08[CFG] received end entity cert "C=CN, O=GM-Test, CN=Initiator SignCert"
08[CFG] Early Gating: SM2 certificate trust chain verified successfully.
10[IKE] extracting encapsulated seed from KE payload (ID 60001)
10[IKE] decapsulating SM2 enveloped data using local EncCert private key
14[IKE] verifying ML-DSA signature over transcript hash (IntAuth)
```

**问题**:
1. 这些日志格式是编造的
2. "ID 60001" 应该是 "ID 1051"
3. ML-DSA 签名验证尚未实现

**修正建议**: 从实际测试中提取真实日志

---

### 12. 早期门控机制描述过度

**草稿原文**:
> "一旦底层返回 AUTH_FAILED，协议栈立即主动销毁 ike_sa 对象"
> "平均耗时仅为 34.2 ms"

**问题**:
1. 早期门控的详细实现需要验证
2. 34.2ms 数据是编造的
3. 实际代码中证书验证是在标准流程中进行的

**修正建议**: 删除或简化这部分描述，待实际测试验证

---

### 13. 分片机制描述

**草稿原文**:
> "证明应用层分片机制被正确触发并运作良好"
> "证明 IKEv2 分片（RFC 7383）与抗 DoS 机制"

**问题**: 未实际测试分片场景

**修正建议**: 删除或标注待测试

---

## 📋 问题汇总表

| 序号 | 问题类型 | 位置 | 草稿内容 | 实际情况 | 严重程度 |
|------|---------|------|---------|---------|---------|
| 1 | Transform ID | 全文 | 60001 | 1051 | 🔴 严重 |
| 2 | 插件名称 | 5.1.2 | gmssl_plugin | gmalg | 🔴 严重 |
| 3 | SM4 模式 | 5.2.1 | GCM, aead_t | ECB/CBC/CTR, crypter_t | 🔴 严重 |
| 4 | 后量子库 | 5.1.1, 5.2.1 | liboqs, oqs | ml 插件 | 🔴 严重 |
| 5 | 性能数据 | 5.4.2 | 21.5ms/98.4ms | 48ms/52ms | 🔴 严重 |
| 6 | strongSwan 版本 | 表5-1 | 6.0.0 | 6.0.4 | 🟡 中等 |
| 7 | 内核版本 | 表5-1 | 5.15 | 6.8.0 | 🟡 中等 |
| 8 | CPU 信息 | 表5-1 | i7-12700K | 待确认 | 🟡 中等 |
| 9 | 通信开销 | 表5-2 | 编造数据 | 待实测 | 🟡 中等 |
| 10 | 后量子认证 | 5.2.4 | 已实现 | 未实现 | 🟢 轻微 |
| 11 | 日志输出 | 5.3.2 | 编造 | 待提取 | 🟢 轻微 |
| 12 | 早期门控 | 5.4.2 | 34.2ms | 编造 | 🟢 轻微 |
| 13 | 分片机制 | 5.4.1 | 已验证 | 未测试 | 🟢 轻微 |

---

## ✅ 正确的内容（可保留）

以下内容与实际实现一致：

1. **协议流程描述**: 5-RTT 流程（IKE_SA_INIT → IKE_INTERMEDIATE #0/#1/#2 → IKE_AUTH）
2. **双证书机制**: SignCert/EncCert 分离
3. **RFC 9242/9370**: 基于 strongSwan 的框架描述
4. **SM3 哈希**: 正确描述了 hasher_t 接口
5. **ML-KEM Transform ID**: 35 (ml-kem-768) 是正确的
6. **SM2-KEM 双向封装**: r_i || r_r 的设计描述
7. **密钥派生公式**: RFC 9370 的级联更新描述

---

## 📝 修改建议

### 高优先级（必须修改）

1. **全局替换**:
   - `60001` → `1051`
   - `gmssl_plugin` → `gmalg 插件`
   - `oqs 插件` → `ml 插件`
   - `liboqs` → 删除或改为 "strongSwan 内置 ml 插件"

2. **SM4 描述修改**:
   ```
   原文: "实现 aead_t 接口封装 sm4_gcm_encrypt()"
   改为: "实现 crypter_t 接口封装 SM4 ECB/CBC/CTR 模式"
   ```

3. **删除编造的性能数据**:
   - 表 5-3 时延数据
   - 早期门控 34.2ms
   - 或标注 "待实测"

### 中优先级（建议修改）

4. **更新环境信息**:
   - strongSwan 版本: 6.0.0 → 6.0.4
   - 内核版本: 5.15 → 6.8.0
   - 运行 `lscpu` 确认 CPU 信息

5. **通信开销数据**:
   - 通过实际抓包获取
   - 或使用参考值并标注 "理论估算"

### 低优先级（可选修改）

6. **日志输出**: 从实际测试提取
7. **删除未实现功能的描述**: 如 ML-DSA 认证、分片验证

---

## 🔍 实际实现对照参考

### 算法 ID 定义（gmalg_plugin.h）
```c
#define HASH_SM3        1032    // SM3 Hash
#define ENCR_SM4_ECB    1040    // SM4 ECB mode
#define ENCR_SM4_CBC    1041    // SM4 CBC mode
#define ENCR_SM4_CTR    1042    // SM4 CTR mode
#define SIGN_SM2        1050    // SM2 Signature
#define KE_SM2          1051    // SM2 Key Exchange
#define PRF_SM3         1052    // PRF with SM3
```

### 实际性能数据（test_results.md）
| 算法 | 性能 |
|------|------|
| SM3 Hash | 443 MB/s |
| SM3 PRF | 3.7M ops/s |
| SM4 ECB | 189 MB/s |
| SM4 CBC | 175 MB/s |

### ML-KEM 混合测试（pqgm-test）
| 配置 | RTT | 时延 |
|------|-----|------|
| x25519 | 2 | 48 ms |
| x25519 + ML-KEM-768 | 3 | 52 ms |

---

*审查完成时间: 2026-02-27*
*建议在修改后进行二次审查*
