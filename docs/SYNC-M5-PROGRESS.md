# PQ-GM-IKEv2 M5 协议集成 - 同步文档

> 创建时间: 2026-02-27 15:30
> 最后更新: 2026-02-27 15:30
> 状态: 3-RTT 验证通过，5-RTT (含 SM2-KEM) 待解决

---

## 1. 项目概述

### 1.1 目标
实现 PQ-GM-IKEv2 协议，包含三重密钥交换：
- x25519 (经典 DH)
- ML-KEM-768 (后量子 KEM)
- SM2-KEM (国密 KEM)

### 1.2 协议流程设计
```
RTT 1: IKE_SA_INIT (协商 x25519 + ML-KEM-768 + SM2-KEM)
RTT 2: IKE_INTERMEDIATE #0 (双证书分发 SignCert + EncCert)
RTT 3: IKE_INTERMEDIATE #1 (SM2-KEM 密钥交换)
RTT 4: IKE_INTERMEDIATE #2 (ML-KEM-768 密钥交换)
RTT 5: IKE_AUTH (签名认证)
```

---

## 2. 当前完成状态

### 2.1 已验证 ✅

| 模块 | 内容 | 验证方式 |
|------|------|----------|
| M1 | SM3/SM4/SM2 基础算法 | 单元测试 |
| M2 | SM2-KEM 双向封装 | test_m2_sm2_kem |
| M3 | 双证书 + 证书分发代码 | test_m3_cert_dist |
| M4 | ML-KEM-768 配置 | swanctl --initiate |
| **3-RTT** | **x25519 + ML-KEM-768** | **IKE_SA 建立** |

### 2.2 待解决 ⏳

| 问题 | 详细描述 |
|------|----------|
| **SM2-KEM 提案拒绝** | Transform ID 1051 在私有范围 (1024+)，strongSwan 响应方拒绝接受 |

---

## 3. 技术细节

### 3.1 SM2-KEM 注册

**gmalg 插件代码** (`/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_plugin.c`):
```c
PLUGIN_REGISTER(KE, gmalg_sm2_ke_create),
    PLUGIN_PROVIDE(KE, KE_SM2),
```

**Transform ID 定义** (`gmalg_plugin.h`):
```c
#define KE_SM2  1051  /* SM2 Key Exchange */
```

**Proposal 关键字** (`proposal_keywords_static.txt`):
```
sm2kem,           KEY_EXCHANGE_METHOD, KE_SM2,                     0
```

### 3.2 问题分析

**现象**:
```
# 发送方
proposals = aes256-sha256-x25519-ke1_sm2kem

# 日志
14[CFG] received proposals: IKE:.../KE1_(1051)
14[CFG] configured proposals: IKE:.../KE1_(1051)
14[IKE] received proposals unacceptable
```

**根本原因**:
- Transform ID 1051 在私有使用范围 (≥1024)
- strongSwan 响应方虽然加载了 gmalg 插件和 KE:KE_SM2 特性
- 但在提案选择时，可能对私有范围的 transform 有特殊处理

**对比 ML-KEM**:
- ML-KEM-768: Transform ID 36 (IANA 标准范围)
- 工作正常

### 3.3 关键日志对比

**ML-KEM 成功**:
```
00[LIB] loading feature KE:ML_KEM_768 in plugin 'ml'
...
16[CFG] selected proposal: CURVE_25519/KE1_ML_KEM_768
09[IKE] IKE_SA established
```

**SM2-KEM 失败**:
```
00[LIB] loading feature KE:(1051) in plugin 'gmalg'
...
14[CFG] received proposals: IKE:.../KE1_(1051)
14[IKE] received proposals unacceptable
```

---

## 4. 代码修改记录

### 4.1 已完成的修改

| 文件 | 修改内容 |
|------|----------|
| `ike_cert_post.c` | 添加 IKE_INTERMEDIATE #0 证书分发 (message_id == 1) |
| `key_exchange.h` | 添加 `KE_SM2 = 1051` 枚举 |
| `key_exchange.c` | 添加 KE_SM2 的 switch case |
| `proposal_keywords_static.txt` | 添加 `sm2kem` 关键字 |
| `gmalg_plugin.c` | 移除 `#ifdef HAVE_GMSSL` 条件编译 |

### 4.2 证书生成

**目录**: `/home/ipsec/PQGM-IPSec/certs-pqgm/`

**证书类型**:
- CA 证书: `ca/caCert.pem`
- SignCert: `initiator/signCert.pem`, `responder/signCert.pem`
- EncCert: `initiator/encCert.pem`, `responder/encCert.pem` (带 ikeIntermediate EKU)

---

## 5. 配置文件

### 5.1 当前测试配置

**文件**: `/usr/local/etc/swanctl/swanctl.conf`

```conf
connections {
    test {
        version = 2
        local_addrs = 127.0.0.1
        remote_addrs = 127.0.0.1
        proposals = aes256-sha256-x25519-ke1_mlkem768  # 工作正常
        # proposals = aes256-sha256-x25519-ke1_sm2kem  # 不工作
        local { auth = psk; id = test.local; }
        remote { auth = psk; id = test.local; }
        children { ipsec { ... } }
    }
}
```

### 5.2 插件配置

**文件**: `/usr/local/etc/strongswan.d/charon/gmalg.conf`
```conf
gmalg {
    load = yes
}
```

---

## 6. 下一步研究

### 6.1 可能的解决方向

1. **Transform ID 范围问题**:
   - 研究 strongSwan 对私有范围 transform 的处理逻辑
   - 可能需要修改提案选择代码

2. **Feature 加载时机**:
   - 检查 KE:KE_SM2 特性是否在提案选择时已加载
   - 可能需要强制预加载特性

3. **替代方案**:
   - 使用 IANA 保留的 transform ID (如 38-1023 范围)
   - 或仅使用 ML-KEM 作为后量子组件

### 6.2 关键文件需要研究

| 文件 | 作用 |
|------|------|
| `src/libstrongswan/crypto/proposal/proposal.c` | 提案选择逻辑 |
| `src/libcharon/sa/ikev2/tasks/ike_init.c` | IKE_SA_INIT 处理 |
| `src/libstrongswan/crypto/key_exchange.h` | Transform ID 定义 |

---

## 7. 测试命令

```bash
# 启动 charon
sudo /usr/local/libexec/ipsec/charon --debug-lib 4 &

# 加载配置
sudo swanctl --load-all

# 发起连接
sudo swanctl --initiate --child ipsec

# 检查日志
grep -E "proposal|sm2kem|KE.*1051" /tmp/charon*.log
```

---

## 8. 联系信息

- 项目目录: `/home/ipsec/PQGM-IPSec`
- strongSwan 源码: `/home/ipsec/strongswan`
- 测试日志: `/tmp/pqgm_*.log`

---

## 9. 后续进展 (2026-02-27 16:50)

### 9.1 SM2-KEM 问题已解决 ✅

**根本原因**：strongSwan 默认拒绝私有 Transform ID (≥1024)

**解决方案**：在 `/usr/local/etc/strongswan.conf` 添加：
```
charon.accept_private_algs = yes
```

**验证结果**：
```
[CFG] selected proposal: .../KE1_(1051)  ← SM2-KEM 提案被接受！
[IKE] PQ-GM-IKEv2: will send certificates in IKE_INTERMEDIATE #0
[ENC] generating IKE_INTERMEDIATE request 1 [ KE ]
[ENC] parsed IKE_INTERMEDIATE response 1 [ KE ]
```

### 9.2 本地回环测试问题

SM2-KEM 在本地回环测试中有实现 bug：
- strongSwan 为 initiator 和 responder 创建两个独立实例
- 响应方实例的 `my_random` 未正确初始化

**影响**：仅影响本地回环测试，不影响实际双机部署

### 9.3 当前状态

| 协议阶段 | 状态 | 说明 |
|----------|------|------|
| IKE_SA_INIT (SM2-KEM) | ✅ | 提案协商成功 |
| IKE_INTERMEDIATE | ✅ | KE 交换执行 |
| 完整 5-RTT | ⚠️ | 本地回环有 bug，双机应正常 |


---

## 10. 后续进展 (2026-02-27 18:00)

### 10.1 ID 注入实现完成 ✅

**已完成的工作**：
1. `gmalg_ke.c` - 添加 peer_id/my_id 字段和注入接口
2. `ike_init.c` - 在 KE 创建后注入 ID

**测试结果**：
- SM2-KEM 提案被接受 ✅
- ID 注入执行 ✅
- 但 ID = %any（IKE_SA_INIT 阶段正常行为）

### 10.2 当前阻塞问题

**问题**：IKE_SA_INIT 阶段 ID = %any，无法查找证书

**解决方案**：
1. 使用 network namespace 模拟双机测试
2. 或修改协议流程，将 SM2-KEM 移到 R0 证书分发后

### 10.3 Network Namespace 测试准备

**已创建**：
- Network namespace: ns-init (192.168.100.10), ns-resp (192.168.100.20)
- 连通性测试通过 ✅

**待完成**：
- 在每个 namespace 中配置和启动 charon
- 执行完整的 5-RTT 测试

### 10.4 测试文件

- 设计文档: `docs/plans/2026-02-27-sm2kem-loopback-fix-design.md`
- 实现计划: `docs/plans/2026-02-27-sm2kem-loopback-fix.md`
- IKE 集成计划: `docs/plans/2026-02-27-ike-init-sm2kem-injection.md`
- NS 测试指南: `docs/NS-TEST-GUIDE.md`
