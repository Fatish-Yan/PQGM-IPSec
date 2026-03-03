# PQ-GM-IKEv2 项目完整文档

> **项目名称**: 抗量子与国密融合的 IKEv2/IPSec 协议设计与实现
> **项目类型**: 硕士毕业设计
> **当前阶段**: 第5章 系统实现与性能评估
> **最后更新**: 2026-02-26

---

## 目录
1. [项目概述](#1-项目概述)
2. [研究背景与目标](#2-研究背景与目标)
3. [协议设计方案](#3-协议设计方案)
4. [开发环境](#4-开发环境)
5. [实现架构](#5-实现架构)
6. [实现状态](#6-实现状态)
7. [错误避坑记录](#7-错误避坑记录)
8. [测试工具与命令](#8-测试工具与命令)
9. [性能测试结果](#9-性能测试结果)
10. [下一步计划](#10-下一步计划)

---

## 1. 项目概述

### 1.1 研究主题
**抗量子与国密融合的 IKEv2 协议设计与实现** - 面向量子计算威胁的 IPsec VPN 安全增强方案

### 1.2 核心创新
- **混合密钥交换**: 经典 DH + 后量子 KEM (ML-KEM) + 国密 SM2-KEM
- **双证书机制**: SM2 签名证书/加密证书分离 + 后量子认证证书
- **基于 RFC 9242/9370**: 利用 IKE_INTERMEDIATE 和多重密钥交换框架

### 1.3 论文结构
- 第1-4章: 已完成（见 `PQGM_IPSec.pdf`）
- **第5章**: 系统实现与性能评估（当前进行中，需用真实数据替换草稿）

---

## 2. 研究背景与目标

### 2.1 威胁模型
- **SNDL (Store-Now-Decrypt-Later)**: 攻击者录制当前流量，等待量子计算机破解
- **主动攻击**: 传统签名算法被量子破解后的中间人攻击
- **DoS 放大**: 后量子算法大 payload 带来的拒绝服务风险

### 2.2 设计目标
1. **抗量子机密性**: 通过混合密钥交换抵御 SNDL
2. **抗量子身份认证**: 通过后量子签名证书保证认证安全
3. **国密特色**: 引入 SM2 双证书与"基于加密证书的密钥交换"
4. **兼容与可实现**: 复用 RFC 9242/9370 扩展点
5. **可评估**: 在 strongSwan 6.0+ 上实现与测试

### 2.3 技术路线

```
传统 IKEv2         PQ-GM-IKEv2 (本方案)
-----------        --------------------
IKE_SA_INIT        IKE_SA_INIT (协商 x25519 + ML-KEM + SM2-KEM)
       ↓                    ↓
   IKE_AUTH          IKE_INTERMEDIATE #0 (双证书分发)
                          ↓
                     IKE_INTERMEDIATE #1 (SM2-KEM 密钥交换)
                          ↓
                     IKE_INTERMEDIATE #2 (ML-KEM 密钥交换)
                          ↓
                       IKE_AUTH (PQ 签名认证)
```

---

## 3. 协议设计方案

### 3.1 核心协议流程

```
IKE_SA_INIT:
  Initiator → Responder: SAi1(KE=x25519, ADDKE1=sm2-kem, ADDKE2=ml-kem-768),
                        KEi, Ni, N(INTERMEDIATE_EXCHANGE_SUPPORTED)
  Responder → Initiator: SAr1(...), KEr, Nr, N(INTERMEDIATE_EXCHANGE_SUPPORTED)

IKE_INTERMEDIATE #0 (双证书分发):
  Initiator → Responder: SK { CERT(SignCert_i), CERT(EncCert_i) }
  Responder → Initiator: SK { CERT(SignCert_r), CERT(EncCert_r) }

IKE_INTERMEDIATE #1 (SM2-KEM):
  Initiator → Responder: SK { KEi(2) [group=sm2-kem] }
  Responder → Initiator: SK { KEr(2) [group=sm2-kem] }
  共享秘密: SK_sm2 = r_i || r_r (双向封装)

IKE_INTERMEDIATE #2 (ML-KEM-768):
  Initiator → Responder: SK { KEi(1) [group=ml-kem-768] }
  Responder → Initiator: SK { KEr(1) [group=ml-kem-768] }

IKE_AUTH (PQ 认证):
  Initiator → Responder: SK { IDi, CERT(AuthCert), AUTH, ... }
  Responder → Initiator: SK { IDr, CERT(AuthCert), AUTH, ... }
```

### 3.2 双证书机制

| 证书类型 | 用途 | 发送时机 | 算法要求 |
|---------|------|---------|---------|
| **SignCert** | 签名证书 | IKE_INTERMEDIATE #0 | SM2 |
| **EncCert** | 加密证书 | IKE_INTERMEDIATE #0 | SM2 |
| **AuthCert** | 认证证书 | IKE_AUTH | ML-DSA/SLH-DSA (后量子) |

### 3.3 密钥派生 (RFC 9370)

每轮额外密钥交换后更新密钥材料：
```
SKEYSEED(n) = prf(SK_d(n-1), SK(n) | Ni | Nr)
{SK_d | SK_ai | SK_ar | SK_ei | SK_er | SK_pi | SK_pr} = prf+(SKEYSEED(n), Ni | Nr | SPIi | SPIr)
```

其中：
- `SK(n)` 为 ML-KEM 导出的 shared secret
- `SK(n)` 为 SM2-KEM 导出的 `r_i || r_r`

### 3.4 Transform ID 分配

| 组件 | Transform ID | 类型 | 说明 |
|------|-------------|------|------|
| x25519 | 31 | KE (Type 4) | IANA 标准 |
| ml-kem-512 | 35 | KE (Type 4) | IANA 标准 |
| ml-kem-768 | 36 | KE (Type 4) | IANA 标准 |
| ml-kem-1024 | 37 | KE (Type 4) | IANA 标准 |
| **sm2-kem** | **60001** | **ADDKE (Type 6)** | **私有使用** |

---

## 4. 开发环境

### 4.1 系统配置
```bash
OS: Ubuntu 22.04
Kernel: Linux 6.8.0-101-generic
Shell: bash
Sudo 密码: 1574a
```

### 4.2 实验平台
- **两台 VMware 虚拟机** (Ubuntu 22.04)
- 作为 Initiator 和 Responder 测试

### 4.3 核心依赖
| 组件 | 版本 | 用途 |
|------|------|------|
| strongSwan | 6.0.4 | IPSec VPN 实现 |
| GmSSL | 3.1.3 Dev | 国密算法库 (SM2/SM3/SM4) |
| gcc | - | 编译器 |

### 4.4 关键路径
```
strongSwan 源码:    /home/ipsec/strongswan
gmalg 插件:         /home/ipsec/strongswan/src/libstrongswan/plugins/gmalg
项目文档:           /home/ipsec/PQGM-IPSec
参考文档:           /home/ipsec/PQGM-IPSec/参考文档/
  - PQGM_IPSec.pdf          (论文 1-4 章)
  - 第五章 系统实现与性能评估.docx  (草稿，数据待替换)
  - draft--pqc-gm-ikev2-03.md (协议设计稿)
  - 角色定义                 (项目说明)
GmSSL 安装:         /usr/local/lib
```

---

## 5. 实现架构

### 5.1 gmalg 插件结构
```
gmalg/
├── gmalg_plugin.c/h      # 插件入口，算法注册
├── gmalg_hasher.c/h      # SM3 哈希算法
├── gmalg_crypter.c/h     # SM4 分组加密 (ECB/CBC/CTR)
├── gmalg_signer.c/h      # SM2 签名算法
├── gmalg_prf.c/h         # SM3 伪随机函数 (内嵌在 hasher)
├── gmalg_ke.c/h          # SM2-KEM 密钥交换 (待实现)
└── Makefile.am           # 构建配置
```

### 5.2 算法 ID 分配 (strongSwan 私有使用空间)
```c
/* GM/T 0004-2012 SM3 Hash Algorithm */
#define HASH_SM3        1032

/* GM/T 0002-2012 SM4 Block Cipher */
#define ENCR_SM4_ECB    1040
#define ENCR_SM4_CBC    1041
#define ENCR_SM4_CTR    1042

/* GM/T 0003-2012 SM2 Signature Algorithm */
#define AUTH_SM2        1050

/* GM/T 0003-2012 SM2 Key Exchange (KEM) */
#define KE_SM2          1051

/* PRF using SM3 */
#define PRF_SM3         1052
```

### 5.3 strongSwan 插件注册机制
```c
// 在 gmalg_plugin.c 的 get_features() 中注册
static plugin_feature_t f[] = {
    // HASHER
    PLUGIN_REGISTER(HASHER, gmalg_sm3_hasher_create),
        PLUGIN_PROVIDE(HASHER, HASH_SM3),

    // PRF
    PLUGIN_REGISTER(PRF, gmalg_sm3_prf_create),
        PLUGIN_PROVIDE(PRF, PRF_SM3),

    // CRYPTER (需要3个参数: type, algo, keysize)
    PLUGIN_REGISTER(CRYPTER, gmalg_sm4_crypter_create),
        PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_ECB, 16),

    // SIGNER (只需要2个参数)
    PLUGIN_REGISTER(SIGNER, gmalg_sm2_signer_create),
        PLUGIN_PROVIDE(SIGNER, AUTH_SM2),
};
```

---

## 6. 实现状态

### 6.1 已完成 ✅

| 模块 | 算法 | 功能测试 | 性能测试 | 说明 |
|------|------|----------|----------|------|
| gmalg_hasher | SM3 Hash | ✅ 通过 | ✅ 443 MB/s | 256位哈希 |
| gmalg_hasher | SM3 PRF | ✅ 通过 | ✅ 3.7M ops/s | 伪随机函数 |
| gmalg_crypter | SM4 ECB | ✅ 通过 | ✅ 189 MB/s | 电子密码本模式 |
| gmalg_crypter | SM4 CBC | ✅ 通过 | ✅ 161-175 MB/s | 密码分组链接 |
| gmalg_crypter | SM4 CTR | ✅ 通过 | ✅ 待测试 | 计数器模式 |
| gmalg_signer | SM2 Sign | ✅ 通过 | ✅ 待测试 | 签名/验证 |

### 6.1.1 SM2 Signer 测试结果

**已验证功能**:
- SM2 密钥对生成（私钥 → 公钥计算）
- SM2 签名生成（DER 编码 70-72 字节）
- SM2 签名验证
- 错误签名拒绝

**关键修复**:
- DER 编码签名长度处理（71 字节实际长度）
- 公钥自动计算（sm2_z256_point_mul_generator）

### 6.2 待实现 ⏳

| 优先级 | 模块 | 算法 | 说明 | 依赖 |
|--------|------|------|------|------|
| **高** | gmalg_ke | SM2-KEM | 双向封装密钥交换 (Transform ID 60001) | r0证书分发 |
| **高** | - | r0 双证书分发 | IKE_INTERMEDIATE #0 阶段 | strongSwan内部 |
| **中** | - | ML-KEM 集成 | 利用 strongSwan 6.0 ml 插件 | ✅ 已验证 |
| **中** | - | IKE_INTERMEDIATE | 中间交换流程 | ✅ 已验证 |
| **中** | - | PQ Auth 认证 | ML-DSA/SLH-DSA 证书支持 | - |

### 6.2.1 SM2-KEM 实现依赖分析

根据协议设计 (draft--pqc-gm-ikev2-03.md)，SM2-KEM 的实现依赖关系：

```
IKE_INTERMEDIATE #0 (r0阶段 - 必须先实现)
  ├─ 双证书分发: SignCert + EncCert
  ├─ 提前身份认证 (DoS 门控)
  └─ 为 SM2-KEM 提供加密证书公钥
        ↓
IKE_INTERMEDIATE #1 (SM2-KEM)
  ├─ Initiator: 用 Responder.EncCert 公钥加密 r_i
  ├─ Responder: 用 Initiator.EncCert 公钥加密 r_r
  └─ Shared Secret = r_i || r_r
```

**核心问题**:
1. SM2-KEM 需要访问 r0 阶段收到的对端 EncCert
2. 标准 KE 接口不支持"双向封装"模式
3. 需要扩展 strongSwan 证书获取机制

### 6.2.2 已验证的基础功能

从 `pqgm-test/results/final_report.txt` 确认：

| 功能 | 状态 | 数据 |
|------|------|------|
| ML-KEM-768 | ✅ 已测试 | 密文 1184 字节 |
| IKE_INTERMEDIATE | ✅ 已测试 | 3 RTT 完整流程 |
| x25519 + ML-KEM | ✅ 已测试 | 时延 +4ms (8.3%) |
| 通信开销 | ✅ 已测量 | +125% (主要来自 ML-KEM) |

### 6.3 实现里程碑

```
[✅] 阶段1: GmSSL 集成环境搭建
[✅] 阶段2: SM3 哈希与 PRF 实现
[✅] 阶段3: SM4 ECB/CBC 加密实现
[✅] 阶段4: SM2 签名算法实现
[✅] 阶段5: SM4 CTR 模式实现
[⏳] 阶段6: SM2-KEM 密钥交换设计 (调研中)
[   ] 阶段7: r0 双证书分发机制
[   ] 阶段8: ML-KEM 集成配置
[✅] 阶段9: ML-KEM 基础功能验证
[   ] 阶段10: 完整端到端测试
[   ] 阶段11: 性能对比测试
[   ] 阶段12: 论文第5章数据填充
```

### 6.4 当前阻塞问题

**SM2-KEM 实现的"鸡生蛋"问题**:

1. **协议要求**: SM2-KEM 必须使用对端的 EncCert 公钥进行加密
2. **证书位置**: EncCert 在 IKE_INTERMEDIATE #0 (r0) 中发送
3. **执行顺序**: r0 → SM2-KEM → ML-KEM → IKE_AUTH
4. **实现难点**: KE 实例需要访问证书管理器获取对端证书

**可能的解决方案**:
- **方案A**: 简化实现 - 使用临时密钥对，不依赖证书
- **方案B**: 完整实现 - 扩展 strongSwan 证书获取机制
- **方案C**: 分阶段实现 - 先核心算法，后续集成证书

---

## 7. 错误避坑记录

### 7.1 编译相关

#### 坑点 1: HAVE_GMSSL 未定义
**现象**: `#ifdef HAVE_GMSSL` 条件为假，算法未注册
**原因**: configure.ac 中 HAVE_GMSSL 定义位置错误
**解决**: 手动在 `/home/ipsec/strongswan/config.h` 添加 `#define HAVE_GMSSL 1`

#### 坑点 2: PLUGIN_PROVIDE 宏参数数量
**现象**: CRYPTER 需要 3 参数，SIGNER 只需 2 参数
**解决**:
```c
PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_ECB, 16)    // 3参数
PLUGIN_PROVIDE(SIGNER, AUTH_SM2)             // 2参数
```

#### 坑点 3: 单体模式编译链接错误
**原因**: Makefile.am 在单体模式下仍尝试链接 libstrongswan.la
**解决**: 使用条件 LIBADD
```makefile
if MONOLITHIC
libstrongswan_gmalg_la_LIBADD = -L/usr/local/lib -lgmssl
else
libstrongswan_gmalg_la_LIBADD = $(top_builddir)/src/libstrongswan/libstrongswan.la -lgmssl
endif
```

### 7.2 GmSSL 3.1.3 API 相关

#### 坑点 4: sm3_hash 函数不存在
**解决**: 使用 SM3_CTX
```c
SM3_CTX ctx;
sm3_init(&ctx);
sm3_update(&ctx, data, len);
sm3_finish(&ctx, digest);
```

#### 坑点 5: sm2_key_init 函数不存在
**解决**: 使用 memset 初始化
```c
memset(&this->sm2_key, 0, sizeof(SM2_KEY));
```

#### 坑点 6: SM2_DEFAULT_ID_LEN 常量名错误
**正确名称**: `SM2_DEFAULT_ID_LENGTH`

#### 坑点 7: sm2_key_set_private_key 参数类型
**正确用法**:
```c
sm2_key_set_private_key(&this->sm2_key, (const uint64_t*)ptr)
```

#### 坑点 8: memxor 类型冲突
**解决**: 不包含 `<gmssl/mem.h>`，只包含需要的头文件

#### 坑点 9: SM2_PRIVATE_KEY_SIZE 宏冲突
**原因**: GmSSL 定义为 96，与自定义宏冲突
**解决**: 使用前缀 `GMALG_SM2_PRIV_KEY_SIZE`

### 7.3 strongSwan 框架相关

#### 坑点 10: INIT 宏字段顺序
**要求**: 必须与结构体定义顺序一致
```c
INIT(this,
    .public = { .signer_interface = { ... } },
    .has_private_key = FALSE,
    .key_size = GMALG_SM2_PRIV_KEY_SIZE,
);
```

#### 坑点 11: 插件加载后立即卸载
**原因**: 特征数组为空 (HAVE_GMSSL 未定义)
**解决**: 确保 config.h 定义了 HAVE_GMSSL

---

## 8. 测试工具与命令

### 8.1 测试程序
```bash
# 功能测试
/home/ipsec/PQGM-IPSec/test_gmalg

# 性能测试
/home/ipsec/PQGM-IPSec/benchmark_gmalg

# 运行环境
LD_LIBRARY_PATH=/usr/local/lib:/home/ipsec/strongswan/src/libstrongswan/.libs
```

### 8.2 编译命令
```bash
cd /home/ipsec/strongswan
make -j$(nproc)
```

### 8.3 安装命令
```bash
cd /home/ipsec/strongswan
sudo make install          # sudo 密码: 1574a
```

### 8.4 服务管理
```bash
sudo systemctl restart strongswan
# 或
sudo charon-systemd stop
sudo charon-systemd start
```

### 8.5 插件状态检查
```bash
swanctl --stats | grep gmalg
```

---

## 9. 性能测试结果

### 9.1 国密算法性能 (GMALG 插件)

| 算法 | 测试项 | 结果 |
|------|--------|------|
| **SM3 Hash** | 吞吐量 | 443.35 MB/s |
| | 单次哈希 | 0.141 ms |
| | 块大小 | 256 位 (32 字节) |
| **SM3 PRF** | 操作速率 | 3,701,173 次/秒 |
| | 密钥大小 | 32 字节 |
| **SM4 ECB** | 加密速度 | 189.45 MB/s |
| | 解密速度 | 189.94 MB/s |
| **SM4 CBC** | 加密速度 | 161.27 MB/s |
| | 解密速度 | 174.76 MB/s |
| **SM4 CTR** | 功能测试 | ✅ 通过 |
| | 性能测试 | ⏳ 待测 |
| **SM2 Sign** | 签名生成 | ✅ 通过 |
| | 签名验证 | ✅ 通过 |
| | 性能测试 | ⏳ 待测 |

### 9.2 ML-KEM 混合密钥交换 (已验证)

从 `pqgm-test/results/final_report.txt`：

| 配置 | 密钥交换方法 | RTT | 平均时延 | 成功率 |
|------|-------------|-----|----------|--------|
| 传统 IKEv2 | x25519 | 2 | 48 ms | 100% |
| 混合密钥交换 | x25519 + ML-KEM-768 | 3 | 52 ms | 100% |

**时延增加**: 约 4ms (8.3%)

### 9.3 通信开销分析

| 报文类型 | 基线 | 混合 | 增量 |
|---------|------|------|------|
| IKE_SA_INIT 请求 | 240 | 284 | +44 |
| IKE_SA_INIT 响应 | 273 | 317 | +44 |
| IKE_INTERMEDIATE 请求 | N/A | 1268 | +1268 |
| IKE_INTERMEDIATE 响应 | N/A | 1200 | +1200 |
| **总计** | ~2049 | ~4605 | **+2556** |

**通信开销增加**: 约 125% (主要来自 ML-KEM-768 密文 1184 字节)

---

## 10. 下一步计划

### 10.1 短期任务 (本周)
1. ⏳ 安装编译后的 strongSwan (`sudo make install`)
2. ⏳ 测试 SM2 signer 功能
3. ⏳ 实现 SM4 CTR 模式

### 10.2 中期任务 (2-3周)
4. ⏳ 实现 SM2-KEM 密钥交换 (Transform ID 60001)
5. ⏳ 集成 ML-KEM (利用 strongSwan ml 插件)
6. ⏳ 实现 IKE_INTERMEDIATE 流程
7. ⏳ 实现双证书分发机制

### 10.3 长期任务 (论文完成前)
8. ⏳ 端到端双机测试
9. ⏳ 性能对比测试 (与标准 IKEv2)
10. ⏳ 安全分析
11. ⏳ 论文第5章数据替换与完善

---

## 11. 参考资料

### 11.1 RFC 标准
- RFC 7296: IKEv2
- RFC 9242: IKE_INTERMEDIATE
- RFC 9370: Multiple Key Exchanges
- RFC 9881: ML-DSA for X.509
- RFC 9909: SLH-DSA for X.509

### 11.2 NIST 标准
- FIPS 203: ML-KEM
- FIPS 204: ML-DSA
- FIPS 205: SLH-DSA

### 11.3 国密标准
- GM/T 0002-2012: SM4
- GM/T 0003-2012: SM2
- GM/T 0004-2012: SM3

### 11.4 软件文档
- strongSwan 6.0: https://docs.strongswan.org/
- GmSSL 3.1: https://github.com/guanzhi/GmSSL

---

## 12. Git 仓库
```
路径: /home/ipsec/PQGM-IPSec
远程: https://github.com/Fatish-Yan/PQGM-IPSec
分支: main
```

---

**文档版本**: 2.0
**维护者**: Claude + 用户
**最后更新**: 2026-02-26
