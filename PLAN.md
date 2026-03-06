# PQ-GM-IKEv2 实现计划

> 创建时间: 2026-02-26
> 目标: 完成 gmalg 插件的剩余功能实现和测试

---

## Phase 0: 文档发现与验证

### 0.1 strongSwan 插件开发文档
**需要查阅的文档：**
- [ ] strongSwan 插件架构: `/home/ipsec/strongswan/src/libstrongswan/plugins/README`
- [ ] 现有插件示例: `/home/ipsec/strongswan/src/libstrongswan/plugins/openssl/`
- [ ] KE 接口定义: `/home/ipsec/strongswan/src/libstrongswan/crypto/crypto_factory.h`

### 0.2 GmSSL 3.1.1 API 文档
**需要验证的 API：**
```c
// SM2-KEM 相关 (待验证)
int sm2_kem_encap(SM2_KEY *key, ...);
int sm2_kem_decap(SM2_KEY *key, ...);

// SM4 CTR 模式 (待验证)
int sm4_ctr_encrypt_blocks(SM4_KEY *key, uint8_t ctr[16], ...);
```

**文档位置：**
- `/usr/local/include/gmssl/sm2.h`
- `/usr/local/include/gmssl/sm4.h`

### 0.3 允许使用的 API 列表
```c
// 已验证可用的 API
SM3_CTX, sm3_init(), sm3_update(), sm3_finish()
SM4_KEY, sm4_set_encrypt_key(), sm4_set_decrypt_key()
sm4_encrypt_blocks(), sm4_decrypt_blocks(), sm4_cbc_encrypt_blocks(), sm4_cbc_decrypt_blocks()
SM2_KEY, sm2_sign(), sm2_verify()
sm2_private_key_info_from_der(), sm2_public_key_info_from_der()
sm2_key_set_private_key()
```

### 0.4 反模式警告
- ❌ 不要使用 `sm3_hash()` (不存在)
- ❌ 不要使用 `sm2_key_init()` (不存在)
- ❌ 不要包含 `<gmssl/mem.h>` (memxor 冲突)
- ❌ 不要在宏定义行内使用注释

---

## Phase 1: 安装与基础验证

### 1.1 目标
安装编译好的 strongSwan 并验证 gmalg 插件加载正常

### 1.2 步骤
```bash
# 1. 安装
cd /home/ipsec/strongswan
echo "1574a" | sudo -S make install

# 2. 验证插件加载
sudo swanctl --stats | grep gmalg

# 3. 检查 charon 日志
journalctl -u strongswan -n 50 | grep -i gmalg
```

### 1.3 验证清单
- [ ] `/usr/local/lib/ipsec/libstrongswan.so` 包含 gmalg 符号
- [ ] `swanctl --stats` 显示 gmalg 在 loaded plugins 中
- [ ] charon 日志无 gmalg 相关错误

### 1.4 参考文件
- `PROJECT.md` 第 8 节 - 测试工具与命令
- `CLAUDE.md` - Build Commands 部分

---

## Phase 2: SM2 Signer 功能测试

### 2.1 目标
验证 SM2 签名算法在 strongSwan 中正常工作

### 2.2 步骤
```bash
# 1. 创建 SM2 测试程序
cd /home/ipsec/PQGM-IPSec
# 编译测试程序（已在 test_gmalg.c 中）

# 2. 运行 SM2 签名测试
LD_LIBRARY_PATH=/usr/local/lib:/home/ipsec/strongswan/src/libstrongswan/.libs \
./test_gmalg
```

### 2.3 测试用例
1. 生成 SM2 密钥对
2. 使用私钥签名测试数据
3. 使用公钥验证签名
4. 验证错误签名被拒绝

### 2.4 验证清单
- [ ] SM2 签名生成成功
- [ ] SM2 签名验证成功
- [ ] 错误签名被正确拒绝

---

## Phase 3: SM4 CTR 模式实现

### 3.1 目标
在 gmalg 插件中添加 SM4 CTR 模式支持

### 3.2 实现步骤

**Step 1: 添加到现有 gmalg_crypter.c**
```c
// 在 gmalg_crypter.c 中添加 SM4_CTR 分支
case SM4_MODE_CTR:
    if (iv.len < SM4_BLOCK_SIZE) return FALSE;
    uint8_t ctr[SM4_BLOCK_SIZE];
    memcpy(ctr, iv.ptr, SM4_BLOCK_SIZE);
    sm4_ctr_encrypt_blocks(&this->enc_key, ctr, in, nblocks, out);
    break;
```

**Step 2: 注册到插件**
```c
// 在 gmalg_plugin.c 中添加
PLUGIN_REGISTER(CRYPTER, gmalg_sm4_ctr_crypter_create),
    PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_CTR, 16),
```

**Step 3: 创建工厂函数**
```c
// 在 gmalg_crypter.c 末尾添加
gmalg_sm4_crypter_t* gmalg_sm4_ctr_crypter_create(encryption_algorithm_t algo, size_t key_size)
{
    if (algo != ENCR_SM4_CTR) return NULL;
    return gmalg_sm4_crypter_create_generic(algo, key_size, SM4_MODE_CTR);
}
```

**Step 4: 更新头文件**
```c
// 在 gmalg_crypter.h 中添加声明
gmalg_sm4_crypter_t* gmalg_sm4_ctr_crypter_create(encryption_algorithm_t algo, size_t key_size);
```

### 3.3 验证
```bash
# 编译
cd /home/ipsec/strongswan && make -j$(nproc)

# 测试
./test_gmalg  # 应包含 SM4 CTR 测试
```

### 3.4 参考文件
- `gmalg_crypter.c` - 现有 ECB/CBC 实现
- `gmalg_plugin.c` - 插件注册模式

---

## Phase 4: SM2-KEM 密钥交换实现

### 4.1 目标
实现 SM2-KEM 作为 strongSwan 的 Key Exchange Method

### 4.2 前置条件分析

**已完成验证**:
- ✅ strongSwan 6.0 ML-KEM 功能正常 (pqgm-test/results/final_report.txt)
- ✅ IKE_INTERMEDIATE 机制工作正常
- ✅ GmSSL sm2_encrypt/sm2_decrypt API 可用

**SM2-KEM 协议依赖**:
```
IKE_INTERMEDIATE #0 (r0) → 双证书分发 (SignCert + EncCert)
        ↓
IKE_INTERMEDIATE #1 → SM2-KEM (使用 EncCert 公钥加密)
```

### 4.3 设计决策点

**问题**: SM2-KEM 需要访问对端 EncCert，但标准 KE 接口无此能力

**方案对比**:

| 方案 | 优点 | 缺点 | 优先级 |
|------|------|------|--------|--------|
| **A. 简化版** | 无需证书，独立可测 | 不符合协议设计 | 低 |
| **B. 完整版** | 符合协议，功能完整 | 实现复杂度高 | 高 |
| **C. 分阶段** | 先核心后集成 | 需要两轮实现 | 中 |

**推荐**: 先实现方案 A 验证核心逻辑，后续升级到方案 B

### 4.4 GmSSL API 验证

**Step 1: 创建 gmalg_ke.c/h**
```c
// gmalg_ke.h
typedef struct gmalg_sm2_ke_t gmalg_sm2_ke_t;

struct gmalg_sm2_ke_t {
    ke_t public;
};

gmalg_sm2_ke_t* gmalg_sm2_ke_create(key_exchange_method_t method);
```

**Step 2: 实现 ke_t 接口**
```c
// 需要实现的方法
METHOD(ke_t, get_method, key_exchange_method_t, ...)
METHOD(ke_t, get_shared_secret, bool, ...)
METHOD(ke_t, set_public_key, bool, ...)
METHOD(ke_t, get_public_key, chunk_t, ...)
METHOD(ke_t, destroy, void, ...)
```

**Step 3: 双向封装实现 (参考 draft--pqc-gm-ikev2-03.md)**
```c
// Initiator: 生成 r_i，用 Responder 公钥加密
// Responder: 解密得 r_i，生成 r_r，用 Initiator 公钥加密
// 共享秘密: SK_sm2 = r_i || r_r
```

**Step 4: 注册到插件**
```c
PLUGIN_REGISTER(KE, gmalg_sm2_ke_create),
    PLUGIN_PROVIDE(KE, KE_SM2),
```

### 4.4 Transform ID
```c
#define KE_SM2  60001  // 私有使用范围
// 注意: strongSwan 的 KE 类型可能需要不同的注册方式
```

### 4.5 验证
- [ ] 编译通过
- [ ] KE 方法在 strongSwan 中可见
- [ ] 双向封装测试通过

### 4.6 参考文件
- `参考文档/draft--pqc-gm-ikev2-03.md` 第 6 节
- `/home/ipsec/strongswan/src/libstrongswan/crypto/ke/` - 现有 KE 实现

---

## Phase 5: 端到端测试

### 5.1 目标
在双机环境中测试完整的 PQ-GM-IKEv2 协议

### 5.2 测试配置
```bash
# Initiator (VM1)
# swanctl.conf
connections {
  pqgm {
    proposals = aes256gcm16-prfsha256-x25519-ke1_mlkem768-ke2_sm2kem
    ...
  }
}

# Responder (VM2)
# 相同配置
```

### 5.3 测试场景
1. 基础连通性测试
2. IKE_SA 建立
3. 密钥交换验证
4. ESP 数据传输

### 5.4 抓包验证
```bash
tcpdump -i eth0 -w pqgm_ike.pcap udp port 500 or udp port 4500
```

---

## Phase 6: 性能评估与论文数据收集

### 6.1 性能指标
- 密钥交换延迟
- 吞吐量
- CPU 使用率
- 内存占用

### 6.2 对比测试
- 标准 IKEv2 (x25519)
- PQ-GM-IKEv2 (x25519 + ML-KEM + SM2-KEM)

### 6.3 论文数据
- 更新 `第五章 系统实现与性能评估.docx`
- 用真实数据替换草稿中的编造数据

---

## 执行顺序

```
[✅] Phase 0: 文档发现与验证
[✅] Phase 1: 安装与基础验证
[✅] Phase 2: SM2 Signer 功能测试
[✅] Phase 3: SM4 CTR 模式实现
[⏳] Phase 4: SM2-KEM 密钥交换实现 (调研中)
[   ] Phase 4a: SM2-KEM 核心算法 (简化版)
[   ] Phase 4b: r0 双证书分发机制
[   ] Phase 4c: SM2-KEM 证书集成
[✅] Phase 5: ML-KEM 基础功能验证
[   ] Phase 6: 完整端到端测试
[   ] Phase 7: 性能评估与论文数据收集
```

### 更新说明

**2026-02-26 更新**:
- 确认 ML-KEM 和 IKE_INTERMEDIATE 基础功能已验证正常
- SM2-KEM 实现需要先解决 r0 证书分发的依赖问题
- 正在评估 SM2-KEM 实现方案（简化版 vs 完整版）

---

## 注意事项

1. **每个 Phase 完成后进行 git 提交**
2. **遇到问题时更新 PROJECT.md 的错误避坑记录**
3. **优先完成可用的论文数据收集**
4. **不确定时先查阅文档再实现**
