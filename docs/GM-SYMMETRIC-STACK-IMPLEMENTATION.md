# 国密对称栈 (GM Symmetric Stack) 实现记录

> **实现日期**: 2026-03-04
> **目标**: 在保持5-RTT协议流程不变的前提下，将IKE/ESP提案中的对称加密和杂凑算法替换为国密算法(SM4/SM3)

---

## 一、背景与目标

### 1.1 背景

当前PQ-GM-IKEv2项目已完成5-RTT完整流程实现，使用的baseline提案是：
```
aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768
```

对称/杂凑栈使用标准算法：
- **IKE加密**: AES-256-CBC
- **IKE完整性**: HMAC-SHA2-256-128
- **IKE PRF**: PRF_HMAC_SHA2_256
- **ESP加密**: AES-256-GCM-16 (AEAD)
- **ESP完整性**: (AEAD无单独完整性)

### 1.2 目标

新增"国密对称栈"版本，用于对比测试国密对称算法与标准算法的性能：

| 组件 | Baseline | GM Symmetric |
|------|----------|--------------|
| IKE加密 | AES-256-CBC | SM4-CBC |
| IKE完整性 | HMAC-SHA2-256-128 | HMAC-SM3-128 |
| IKE PRF | PRF_HMAC_SHA2_256 | PRF_SM3 |
| ESP加密 | AES-256-GCM-16 | SM4-CBC |
| ESP完整性 | (AEAD) | HMAC-SM3-128 |

**保持不变**:
- 协议流程 (5-RTT)
- 主DH (x25519)
- KE1 (SM2-KEM)
- KE2 (ML-KEM-768)
- 认证 (ECDSA + ML-DSA-65 混合证书)

---

## 二、修改文件清单

### 2.1 strongSwan 核心修改

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `crypto/proposal/proposal_keywords_static.txt` | 新增 | 添加sm4/hmacsm3/prfsm3提案关键字 |
| `crypto/crypters/crypter.h` | 新增 | 添加ENCR_SM4_ECB/CBC/CTR枚举 |
| `crypto/prfs/prf.h` | 新增 | 添加PRF_SM3枚举 |
| `crypto/hashers/hasher.c` | 修改 | 添加PRF_SM3到hasher_algorithm_from_prf() |
| `crypto/iv/iv_gen.c` | 修改 | 添加ENCR_SM4_*到IV生成器选择 |
| `plugins/gmalg/gmalg_hasher.h` | 修改 | 修复gmalg_sm3_prf_create函数签名 |
| `plugins/gmalg/gmalg_hasher.c` | 修改 | 修复PRF实现和空密钥处理 |

### 2.2 配置文件修改

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `docker/initiator/config/swanctl.conf` | 新增 | 添加pqgm-5rtt-gm-symm连接配置 |
| `docker/responder/config/swanctl.conf` | 新增 | 添加pqgm-5rtt-gm-symm连接配置 |

---

## 三、详细修改内容

### 3.1 提案关键字注册 (`proposal_keywords_static.txt`)

```diff
 sm2kem,           KEY_EXCHANGE_METHOD, KE_SM2,                     0
+sm4,              ENCRYPTION_ALGORITHM, ENCR_SM4_CBC,             128
+sm4cbc,           ENCRYPTION_ALGORITHM, ENCR_SM4_CBC,             128
+hmacsm3,          INTEGRITY_ALGORITHM,  AUTH_HMAC_SM3_128,          0
+hmacsm3_256,      INTEGRITY_ALGORITHM,  AUTH_HMAC_SM3_256,          0
+prfsm3,           PSEUDO_RANDOM_FUNCTION, PRF_SM3,                  0
 noesn,            EXTENDED_SEQUENCE_NUMBERS, NO_EXT_SEQ_NUMBERS,   0
```

**说明**:
- `sm4`/`sm4cbc`: 映射到ENCR_SM4_CBC (1041)，默认密钥长度128位
- `hmacsm3`: 映射到AUTH_HMAC_SM3_128 (1056)，128位截断
- `hmacsm3_256`: 映射到AUTH_HMAC_SM3_256 (1057)，256位完整
- `prfsm3`: 映射到PRF_SM3 (1052)

### 3.2 加密算法枚举 (`crypter.h`)

```c
/* SM4 Chinese national cipher (GM/T 0002-2012) */
ENCR_SM4_ECB =          1040,
ENCR_SM4_CBC =          1041,
ENCR_SM4_CTR =          1042,
```

**位置**: 在`ENCR_AES_CFB = 1030`之后

### 3.3 PRF算法枚举 (`prf.h`)

```c
/** draft-kanno-ipsecme-camellia-xcbc, not yet assigned by IANA */
PRF_CAMELLIA128_XCBC = 1028,
/** SM3 PRF (GM/T 0004-2012) */
PRF_SM3 = 1052,
```

### 3.4 Hasher-PRF映射 (`hasher.c`)

```c
case PRF_HMAC_SHA2_512:
    return HASH_SHA512;
case PRF_SM3:
    return HASH_SM3;
case PRF_HMAC_TIGER:
```

**原因**: PRF_SM3使用SM3哈希，需要添加映射

### 3.5 IV生成器选择 (`iv_gen.c`)

```c
case ENCR_RC2_CBC:
case ENCR_AES_CFB:
case ENCR_SM4_CBC:
    return iv_gen_rand_create();
// ...
case ENCR_CAMELLIA_CCM_ICV16:
case ENCR_SM4_CTR:
case ENCR_CHACHA20_POLY1305:
    return iv_gen_seq_create();
// ...
case ENCR_UNDEFINED:
case ENCR_SM4_ECB:
case ENCR_DES_ECB:
    break;  // No IV needed
```

**说明**:
- SM4-CBC: 随机IV生成器
- SM4-CTR: 序列IV生成器
- SM4-ECB: 无需IV

### 3.6 SM3 PRF函数签名修复 (`gmalg_hasher.h/c`)

**修改前** (错误):
```c
prf_t* gmalg_sm3_prf_create(chunk_t key);
```

**修改后** (正确):
```c
prf_t* gmalg_sm3_prf_create(pseudo_random_function_t algo);
```

**原因**: strongSwan的PRF工厂模式要求创建函数接受`pseudo_random_function_t`参数，而不是`chunk_t key`

### 3.7 SM3 PRF空密钥处理 (`gmalg_hasher.c`)

```c
METHOD(prf_t, get_bytes, bool,
    private_gmalg_sm3_prf_t *this, chunk_t seed, uint8_t *bytes)
{
    SM3_CTX ctx;
    uint8_t digest[SM3_DIGEST_SIZE];
    size_t i;

    for (i = 0; i < seed.len; i += SM3_DIGEST_SIZE)
    {
        size_t len = (seed.len - i) < SM3_DIGEST_SIZE ? (seed.len - i) : SM3_DIGEST_SIZE;

        sm3_init(&ctx);
        /* Handle empty key case - use zero-padded key if not set */
        if (this->key.len > 0 && this->key.ptr)
        {
            sm3_update(&ctx, this->key.ptr, this->key.len);
        }
        sm3_update(&ctx, seed.ptr + i, len);
        sm3_finish(&ctx, digest);

        memcpy(bytes + i, digest, len);
    }

    return TRUE;
}
```

**原因**: 如果在`set_key()`之前调用`get_bytes()`，`key`是`chunk_empty`（ptr=NULL），需要防止空指针解引用

### 3.8 swanctl连接配置 (`docker/*/config/swanctl.conf`)

```conf
# 国密对称栈配置 (SM4-CBC + HMAC-SM3)
# 用于对比测试国密对称算法与标准算法的性能
pqgm-5rtt-gm-symm {
    version = 2
    local_addrs = 172.28.0.10
    remote_addrs = 172.28.0.20

    # IKE SA proposals - 国密对称栈
    # SM4-CBC + HMAC-SM3-128 + PRF-SM3 + x25519+ SM2-KEM+ ML-KEM-768
    proposals = sm4-hmacsm3-prfsm3-x25519-ke1_sm2kem-ke2_mlkem768

    local {
        auth = pubkey
        id = initiator.pqgm.test
        certs = initiator_hybrid_cert.pem
    }

    remote {
        auth = pubkey
        id = responder.pqgm.test
    }

    children {
        net {
            local_ts = 10.1.0.0/16
            remote_ts = 10.2.0.0/16

            # ESP proposals - 国密对称栈
            # SM4-CBC + HMAC-SM3-128 (非 AEAD)
            esp_proposals = sm4-hmacsm3

            start_action = none
        }
    }
}
```

---

## 四、遇到的问题与解决方案

### 4.1 配置文件未被加载

**问题**: 重启Docker容器后，只加载了`pqgm-5rtt-mldsa`连接，没有加载`pqgm-5rtt-gm-symm`

**根因**: Docker-compose.yml挂载的是`./config/swanctl.conf`，而不是`swanctl-5rtt-mldsa.conf`

**解决方案**: 直接修改`swanctl.conf`文件，添加国密对称栈连接配置

### 4.2 提案格式无效

**问题**:
```
loading connection 'pqgm-5rtt-gm-symm' failed: invalid value for: proposals, config discarded
```

**根因**: strongSwan提案解析器不识别数字ID格式`1041-1056-1052-31-ke1_1051-ke2_mlkem768`

**解决方案**: 在`proposal_keywords_static.txt`中添加关键字映射

### 4.3 编译错误 - 未处理的枚举值

**问题1**:
```
crypto/hashers/hasher.c:164:9: error: enumeration value 'PRF_SM3' not handled in switch [-Werror=switch]
```

**解决方案**: 添加`case PRF_SM3: return HASH_SM3;`

**问题2**:
```
crypto/iv/iv_gen.c:28:9: error: enumeration value 'ENCR_SM4_ECB' not handled in switch
```

**解决方案**: 添加SM4加密算法的case分支

### 4.4 PRF函数签名不匹配

**问题**: IKE SA协商时出现SIGSEGV

**根因**: `gmalg_sm3_prf_create(chunk_t key)`签名与strongSwan插件系统期望的`gmalg_sm3_prf_create(pseudo_random_function_t algo)`不匹配

**解决方案**: 修改函数签名，在函数内部检查`algo == PRF_SM3`

### 4.5 空指针解引用风险

**问题**: `get_bytes()`可能在`set_key()`之前被调用

**根因**: `key`初始化为`chunk_empty`（ptr=NULL），直接访问会崩溃

**解决方案**: 添加空指针检查`if (this->key.len > 0 && this->key.ptr)`

---

## 五、测试验证

### 5.1 编译验证

```bash
cd /home/ipsec/strongswan
make clean
./configure --enable-gmalg --enable-mldsa --enable-swanctl --with-gmssl=/usr/local
make -j$(nproc)
sudo make install
```

**结果**: ✅ 编译成功，无警告无错误

### 5.2 配置加载验证

```bash
# 重启Docker容器
sudo docker-compose down
sudo docker-compose up -d

# 加载配置
sudo docker exec pqgm-initiator swanctl --load-all
sudo docker exec pqgm-responder swanctl --load-all
```

**结果**: ✅ 两个连接配置都成功加载
```
loaded connection 'pqgm-5rtt-mldsa'
loaded connection 'pqgm-5rtt-gm-symm'
```

### 5.3 连接测试

```bash
# 测试国密对称栈连接
sudo docker exec pqgm-initiator swanctl --initiate --child net --ike pqgm-5rtt-gm-symm
```

**预期结果**:
- IKE提案协商: SM4-CBC / HMAC-SM3-128 / PRF-SM3 / x25519 / SM2-KEM / ML-KEM-768
- ESP提案协商: SM4-CBC / HMAC-SM3-128
- CHILD_SA成功建立

---

## 六、算法ID汇总

| 类型 | 名称 | ID | 说明 |
|------|------|----|----- |
| ENCRYPTION | ENCR_SM4_ECB | 1040 | SM4 ECB模式 |
| ENCRYPTION | ENCR_SM4_CBC | 1041 | SM4 CBC模式 |
| ENCRYPTION | ENCR_SM4_CTR | 1042 | SM4 CTR模式 |
| INTEGRITY | AUTH_HMAC_SM3_128 | 1056 | HMAC-SM3, 128位截断 |
| INTEGRITY | AUTH_HMAC_SM3_256 | 1057 | HMAC-SM3, 256位完整 |
| PRF | PRF_SM3 | 1052 | SM3伪随机函数 |

---

## 七、提案字符串对照

### 7.1 Baseline (标准算法)

```
IKE: aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768
ESP: aes256gcm16-sha256
```

### 7.2 GM Symmetric (国密算法)

```
IKE: sm4-hmacsm3-prfsm3-x25519-ke1_sm2kem-ke2_mlkem768
ESP: sm4-hmacsm3
```

### 7.3 算法映射

| 字符串 | 算法ID | 说明 |
|--------|--------|------|
| `sm4` | ENCR_SM4_CBC (1041) | SM4-CBC加密 |
| `hmacsm3` | AUTH_HMAC_SM3_128 (1056) | HMAC-SM3完整性 |
| `prfsm3` | PRF_SM3 (1052) | SM3 PRF |

---

## 八、下一步工作

1. **性能对比测试**: 运行多次IKE SA建立，对比baseline和GM symmetric的握手时间
2. **数据收集**: 记录每个RTT的耗时，用于论文系统评估章节
3. **文档更新**: 将实验数据填入`ML-DSA-5RTT-THESIS-DATA.md`

---

## 九、Git提交记录

### strongswan仓库

```
07a9a235e1 feat: add SM4, HMAC-SM3 algorithms for IKE/ESP proposals
```

**修改内容**:
- 添加SM4加密算法枚举和关键字
- 添加HMAC-SM3完整性算法关键字
- 添加PRF-SM3伪随机函数关键字
- 修复PRF函数签名和空密钥处理

### PQGM-IPSec仓库

```
4a0a00f feat: add GM symmetric stack connection profile (SM4+HMAC-SM3)
```

**修改内容**:
- 添加pqgm-5rtt-gm-symm连接配置
- 更新patch文件

---

## 十、参考资料

- **GM/T 0002-2012**: SM4分组密码算法
- **GM/T 0004-2012**: SM3密码杂凑算法
- **RFC 7296**: IKEv2协议
- **strongSwan文档**: https://docs.strongswan.org/
