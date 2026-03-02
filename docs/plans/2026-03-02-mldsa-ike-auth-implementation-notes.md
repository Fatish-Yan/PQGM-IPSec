# ML-DSA IKE_AUTH 集成实现思路与活动记录

**日期**: 2026-03-02
**目的**: 记录实现细节和思路，用于后续论文撰写

---

## 一、项目背景

### 1.1 目标
将 ML-DSA（后量子数字签名算法）集成到 strongSwan 的 IKE_AUTH 认证流程中，实现后量子安全的身份认证。

### 1.2 技术挑战

1. **OpenSSL 3.0.2 不支持 ML-DSA** - 无法生成标准的 ML-DSA X.509 证书
2. **strongSwan 无原生 ML-DSA 支持** - 需要开发自定义插件
3. **私钥格式问题** - ML-DSA 私钥是原始二进制格式，不是 PEM/DER

### 1.3 解决方案架构

```
┌─────────────────────────────────────────────────────────────┐
│                    混合证书方案                              │
├─────────────────────────────────────────────────────────────┤
│  X.509 证书                                                 │
│  ├── SubjectPublicKeyInfo: ECDSA P-256 (占位符)            │
│  ├── 扩展:                                                  │
│  │   ├── SAN: DNS:<name>.pqgm.test                        │
│  │   └── 1.3.6.1.4.1.99999.1.2: ML-DSA-65 公钥 (1952 bytes)│
│  └── 签名: ECDSA-SHA256 (CA 签名)                          │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、实现步骤

### 2.1 Task 1: 创建 swanctl 配置文件

**文件**: `docker/initiator/config/swanctl-mldsa-hybrid.conf`

**配置要点**:
```conf
connections {
    pqgm-mldsa-hybrid {
        local {
            auth = pubkey
            certs = initiator_hybrid_cert.pem
        }
        remote {
            auth = pubkey
            cacerts = mldsa_ca.pem
        }
    }
}

secrets {
    mldsa-key {
        id = initiator.pqgm.test
        file = initiator_mldsa_key.bin
    }
}
```

**关键决策**: 不使用 `type = mldsa` 参数，因为 strongSwan 不支持自定义密钥类型

### 2.2 Task 2: 创建 ML-DSA 私钥加载器

**核心挑战**: 实现 PRIVKEY builder 模式

**函数签名**:
```c
private_key_t *mldsa_private_key_load(key_type_t type, va_list args);
```

**Builder 参数处理**:
```c
switch (part) {
    case BUILD_BLOB:           // 直接数据
    case BUILD_BLOB_PEM:       // PEM 格式数据
    case BUILD_BLOB_ASN1_DER:  // DER 格式数据
    case BUILD_FROM_FILE:      // 文件路径
    case BUILD_KEY_SIZE:       // 密钥大小（跳过）
    case BUILD_END:            // 参数结束
    default:                   // 不支持的参数
}
```

**关键修复**:
1. 添加 `__attribute__((visibility("default")))` 导出函数符号
2. 正确处理 `BUILD_BLOB_PEM` (type=5) 类型

### 2.3 Task 3: 更新插件注册

**注册方式**:
```c
static plugin_feature_t f[] = {
    PLUGIN_REGISTER(SIGNER, mldsa_signer_create),
        PLUGIN_PROVIDE(SIGNER, AUTH_MLDSA_65),
    PLUGIN_REGISTER(PRIVKEY, mldsa_private_key_load, FALSE),
        PLUGIN_PROVIDE(PRIVKEY, KEY_ANY),
        PLUGIN_PROVIDE(PRIVKEY, KEY_MLDSA65),
};
```

**注意**: 使用 `PRIVKEY` 而不是 `PRIVATE_KEY`

### 2.4 Task 4: 编译和部署

**关键问题**: rpath 设置

**解决方案**:
```bash
sudo chrpath -r /usr/local/lib /usr/local/lib/ipsec/plugins/libstrongswan-mldsa.so
```

**原因**: 编译时 rpath 指向源码目录，Docker 容器无法访问

---

## 三、关键技术发现

### 3.1 符号可见性问题

**问题**: 函数编译后符号为本地符号 (t)，无法被动态链接

**诊断**:
```bash
nm -D libstrongswan-mldsa.so | grep mldsa_private_key_load
# 输出: 000000000000b980 t mldsa_private_key_load  # 小写 t 表示本地符号
```

**解决**:
```c
__attribute__((visibility("default")))
private_key_t *mldsa_private_key_load(key_type_t type, va_list args);
```

### 3.2 Builder 模式参数处理

**问题**: 日志显示 `ML-DSA: unknown builder part, aborting`

**分析**: strongSwan 传递了多种 builder part 类型，不只 `BUILD_BLOB` 和 `BUILD_FROM_FILE`

**解决**: 扩展 switch 语句处理更多类型:
- `BUILD_BLOB_PEM` (type=5) - PEM 格式数据
- `BUILD_KEY_SIZE` - 密钥大小参数

### 3.3 private_key_t 接口实现

**必需方法**:
```c
struct private_key_t {
    key_type_t (*get_type)(private_key_t *this);
    bool (*sign)(private_key_t *this, signature_scheme_t scheme, ...);
    bool (*decrypt)(private_key_t *this, encryption_scheme_t scheme, ...);
    int (*get_keysize)(private_key_t *this);
    public_key_t* (*get_public_key)(private_key_t *this);
    bool (*equals)(private_key_t *this, private_key_t *other);
    bool (*belongs_to)(private_key_t *this, public_key_t *public);
    bool (*get_fingerprint)(private_key_t *this, cred_encoding_type_t type, chunk_t *fp);
    bool (*has_fingerprint)(private_key_t *this, chunk_t fp);
    bool (*get_encoding)(private_key_t *this, cred_encoding_type_t type, chunk_t *encoding);
    private_key_t* (*get_ref)(private_key_t *this);
    void (*destroy)(private_key_t *this);
};
```

**错误实现**: 最初使用了 `get_refcount` 方法，但该方法不存在于接口中

**正确实现**: 使用 `get_ref` 方法配合 `refcount_t ref` 成员

---

## 四、验证结果

### 4.1 ML-DSA 私钥加载成功

**日志证据**:
```
05[LIB] ML-DSA: mldsa_private_key_load called, type=0
05[LIB] ML-DSA: got builder part 5
05[LIB] ML-DSA: BUILD_BLOB_* (type 5), len=4032
05[LIB] ML-DSA: got builder part 64
05[LIB] ML-DSA: BUILD_END
05[LIB] ML-DSA: loaded private key successfully
05[CFG] loaded private key from '/usr/local/etc/swanctl/private/initiator_mldsa_key.bin'
```

### 4.2 Builder 数量变化

- **修复前**: `tried 5 builders`
- **修复后**: `tried 7 builders` (增加了 mldsa 插件的两个注册)

---

## 五、待完成工作

### 5.1 IKE 提案匹配

**当前问题**: ~~`NO_PROPOSAL_CHOSEN` 错误~~ ✅ 已解决

**原因分析**: Initiator 和 Responder 的提案配置不匹配

**后续工作**: 调试提案配置

### 5.2 ML-DSA IKE_AUTH 认证

**当前状态**:
- ✅ ML-DSA 私钥加载成功
- ✅ ML-DSA 签名生成成功 (3309 bytes)
- ❌ Responder 端验证失败

**当前问题**: Responder 使用证书中的 ECDSA 公钥验证 ML-DSA 签名

**需要实现**:
1. ~~修改 IKE_AUTH 认证流程识别 ML-DSA 混合证书~~
2. ~~从证书扩展提取 ML-DSA 公钥~~
3. ✅ 使用 ML-DSA 私钥生成签名
4. ❌ 使用 ML-DSA 公钥验证签名 (阻塞中)

**关键文件**: `src/libcharon/sa/ikev2/tasks/ike_auth.c`

---

## 五点五、最新进展 (2026-03-02 下午)

### 5.5.1 签名生成成功

**日志证据**:
```
05[LIB] ML-DSA: found ML-DSA private key via fallback lookup
05[LIB] ML-DSA: sign() called, scheme=23, loaded=1, sig_ctx=0x7a90e00021c0
05[LIB] ML-DSA: signature created successfully, len=3309
```

### 5.5.2 核心修复

#### 修复 1: scheme_map 添加 ML-DSA

**文件**: `src/libstrongswan/credentials/keys/public_key.c`

```c
// 在 scheme_map 数组添加
{ KEY_MLDSA65, 0, { .scheme = SIGN_MLDSA65 }},
```

**作用**: 让 ML-DSA 私钥能够选择正确的签名方案 (SIGN_MLDSA65 = 23)

#### 修复 2: credential_manager 回退查找

**文件**: `src/libstrongswan/credentials/credential_manager.c`

```c
/* Fallback for ML-DSA hybrid certificates */
if (!private && (type == KEY_MLDSA65 || type == KEY_ANY))
{
    enumerator_t *key_enum;
    private_key_t *key;
    key_enum = create_private_enumerator(this, KEY_MLDSA65, NULL);
    if (key_enum)
    {
        while (key_enum->enumerate(key_enum, &key))
        {
            private = key->get_ref(key);
            DBG1(DBG_LIB, "ML-DSA: found ML-DSA private key via fallback lookup");
            break;
        }
        key_enum->destroy(key_enum);
    }
}
```

**作用**: 混合证书场景下，标准指纹查找失败时，直接枚举所有 ML-DSA 私钥

#### 修复 3: Makefile.am OpenSSL 链接

**文件**: `src/libstrongswan/plugins/mldsa/Makefile.am`

```makefile
libstrongswan_mldsa_la_LIBADD = $(liboqs_LIBS) -lssl -lcrypto
```

**作用**: 解决 `CRYPTO_free` 未定义符号问题

### 5.5.3 当前阻塞问题

**问题**: Responder 无法验证 ML-DSA 签名

**根因**:
```
混合证书结构:
├── SubjectPublicKeyInfo: ECDSA P-256 (占位符)  ← Responder 用这个验证
└── 扩展 1.3.6.1.4.1.99999.1.2: ML-DSA 公钥   ← 应该用这个验证
```

**解决方案** (已实现):
1. ✅ 创建 `mldsa_public_key.c/h` 实现 `public_key_t` 接口
2. ✅ 添加 `verify()` 方法使用 ML-DSA 公钥验证签名
3. ✅ 修改 credential_manager.c 识别混合证书并提取正确的公钥

---

## 五点六、最新进展 (2026-03-02 晚)

### 5.6.1 已实现组件

| 组件 | 文件 | 状态 |
|------|------|------|
| ML-DSA 签名器 | mldsa_signer.c/h | ✅ 完成 |
| ML-DSA 私钥加载器 | mldsa_private_key.c/h | ✅ 完成 |
| ML-DSA 公钥加载器 | mldsa_public_key.c/h | ✅ 新增 |
| scheme_map 更新 | public_key.c | ✅ 完成 |
| 私钥回退查找 | credential_manager.c | ✅ 完成 |
| 混合证书公钥提取 | credential_manager.c | ✅ 新增 |

### 5.6.2 测试结果

**签名生成测试**:
```
[LIB] ML-DSA: found ML-DSA private key via fallback lookup
[LIB] ML-DSA: sign() called, scheme=23, loaded=1, sig_ctx=0x78473c019370
[LIB] ML-DSA: signature created successfully, len=3309
[IKE] authentication of 'initiator.pqgm.test' (myself) with (23) failed
```

**状态**:
- ✅ 私钥查找成功
- ✅ 签名生成成功 (3309 bytes)
- ❌ 认证仍然失败 (待调试)

### 5.6.3 待调查问题

**问题**: 签名创建成功但认证失败

**可能原因**:
1. RFC 7427 签名认证数据格式问题
2. 证书编码问题导致公钥提取失败
3. IKE_AUTH 消息发送失败

**Git 提交**: `0fcf4c497b feat(mldsa): implement ML-DSA public key extraction from hybrid certificates`

---

## 六、代码统计

| 文件 | 行数 | 说明 |
|------|------|------|
| mldsa_private_key.c | ~283 | 私钥加载器实现 |
| mldsa_private_key.h | ~31 | 私钥加载器接口 |
| mldsa_public_key.c | ~370 | 公钥加载器实现 (新增) |
| mldsa_public_key.h | ~47 | 公钥加载器接口 (新增) |
| mldsa_plugin.c | ~80 | 插件注册 (更新) |
| swanctl-mldsa-hybrid.conf | ~38 | 配置文件 |

---

## 七、参考资源

- [FIPS 204: ML-DSA Standard](https://csrc.nist.gov/pubs/fips/204/final)
- [liboqs Documentation](https://github.com/open-quantum-safe/liboqs)
- [strongSwan Plugin Development](https://docs.strongswan.org/)

---

*创建时间: 2026-03-02*
