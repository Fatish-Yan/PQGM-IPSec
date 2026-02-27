# SM2-KEM 本地回环测试修复设计

> 创建时间: 2026-02-27
> 状态: 已批准

---

## 1. 问题描述

### 1.1 当前问题

SM2-KEM 在本地回环测试中有 bug：
- strongSwan 为 initiator 和 responder 创建两个独立实例
- 响应方实例的 `my_random` 未正确初始化
- 导致 `compute_shared_secret()` 失败

### 1.2 根本原因

当前 SM2-KEM 实现假设：
1. `get_public_key()` 被调用时，先收到对端公钥，再发送密文
2. `set_public_key()` 被调用时，先收到对端公钥，再收到密文

但在实际流程中：
- R0 阶段已经交换了双证书（SignCert + EncCert）
- R1 阶段直接交换密文，不需要再交换公钥

---

## 2. 设计方案

### 2.1 核心思路

**模仿 DH 交换模式**：
- 双方都发送密文（而不是先公钥再密文）
- 使用 R0 阶段已交换的 EncCert 公钥

### 2.2 密钥流转设计

```
R0: IKE_INTERMEDIATE #0 - 证书分发
    ↓ EncCert 被加载到 lib->creds

R1: IKE_INTERMEDIATE #1 - SM2-KEM
    get_public_key():
      1. 从 lib->creds 查找对端 EncCert 公钥（用对端 ID + 加密用途）
      2. 生成随机数 r
      3. 用对端公钥加密 r → 密文 CT
      4. 返回 CT

    set_public_key(CT):
      1. 收到对端密文 CT
      2. 从 lib->creds 查找本端 Enc 私钥（用本端 ID）
      3. 用私钥解密 CT → 对端随机数
      4. 保存 peer_random
```

### 2.3 共享秘密计算

**关键**：保证双方拼接顺序一致

```
SK = Initiator_Random || Responder_Random

Initiator: SK = my_random || peer_random
Responder: SK = peer_random || my_random
```

---

## 3. 架构设计

### 3.1 不修改全局接口

使用**向下转型（Downcasting）**方案：
- 不修改 `key_exchange_t` 接口
- 在 `private_gmalg_sm2_ke_t` 中添加额外字段
- 通过专属方法注入信息

### 3.2 结构体扩展

```c
struct private_gmalg_sm2_ke_t {
    key_exchange_t public;

    // 原有字段
    key_exchange_method_t method;
    chunk_t my_random;
    chunk_t peer_random;
    chunk_t shared_secret;
    // ... 其他原有字段 ...

    // 新增：注入的信息
    identification_t *peer_id;    // 对端 ID
    identification_t *my_id;      // 本端 ID
    bool is_initiator;            // 角色
};
```

### 3.3 注入方法

```c
// 设置对端 ID
static void set_peer_id(private_gmalg_sm2_ke_t *this, identification_t *peer_id);

// 设置本端 ID
static void set_my_id(private_gmalg_sm2_ke_t *this, identification_t *my_id);

// 设置角色
static void set_role(private_gmalg_sm2_ke_t *this, bool is_initiator);
```

---

## 4. 关键函数设计

### 4.1 `get_public_key()` - 封装

**职责**：用对端公钥加密，返回密文

```c
METHOD(key_exchange_t, get_public_key, bool,
    private_gmalg_sm2_ke_t *this, chunk_t *value)
{
    // 1. 查找对端 EncCert 公钥
    enumerator = lib->creds->create(lib->creds,
        CRED_CERTIFICATE, CERT_X509,
        CERT_SUBJECT, this->peer_id,
        CERT_KEY_USAGE, XKU_KEY_ENCIPHERMENT,
        CRED_END);

    // 2. 生成随机数（使用 strongSwan RNG）
    this->my_random = chunk_alloc(SM2_KEM_RANDOM_SIZE);
    rng_t *rng = lib->crypto->create_rng(lib->crypto, RNG_STRONG);
    rng->get_bytes(rng, this->my_random.len, this->my_random.ptr);
    DESTROY_IF(rng);

    // 3. 用对端公钥加密 → 密文
    sm2_encrypt(peer_pubkey, this->my_random, &ciphertext);

    *value = chunk_clone(ciphertext);
    return TRUE;
}
```

### 4.2 `set_public_key()` - 解封装

**职责**：用本端私钥解密，得到对端随机数

```c
METHOD(key_exchange_t, set_public_key, bool,
    private_gmalg_sm2_ke_t *this, chunk_t value)
{
    // 1. 查找本端 SM2 私钥
    enumerator = lib->creds->create(lib->creds,
        CRED_PRIVATE_KEY, KEY_SM2,
        CERT_SUBJECT, this->my_id,
        CRED_END);

    // 2. 用私钥解密
    sm2_decrypt(my_privkey, value, &plaintext);

    // 3. 保存对端随机数
    this->peer_random = chunk_clone(plaintext);
    return TRUE;
}
```

### 4.3 `get_shared_secret()` - 计算共享秘密

**职责**：保证双方拼接顺序一致

```c
METHOD(key_exchange_t, get_shared_secret, bool,
    private_gmalg_sm2_ke_t *this, chunk_t *secret)
{
    this->shared_secret = chunk_alloc(SM2_KEM_RANDOM_SIZE * 2);

    // SK = Initiator_Random || Responder_Random
    if (this->is_initiator) {
        memcpy(this->shared_secret.ptr,
               this->my_random.ptr, SM2_KEM_RANDOM_SIZE);
        memcpy(this->shared_secret.ptr + SM2_KEM_RANDOM_SIZE,
               this->peer_random.ptr, SM2_KEM_RANDOM_SIZE);
    } else {
        memcpy(this->shared_secret.ptr,
               this->peer_random.ptr, SM2_KEM_RANDOM_SIZE);
        memcpy(this->shared_secret.ptr + SM2_KEM_RANDOM_SIZE,
               this->my_random.ptr, SM2_KEM_RANDOM_SIZE);
    }

    *secret = chunk_clone(this->shared_secret);
    return TRUE;
}
```

### 4.4 `destroy()` - 防止内存泄漏

```c
METHOD(key_exchange_t, destroy, void,
    private_gmalg_sm2_ke_t *this)
{
    chunk_clear(&this->my_random);
    chunk_clear(&this->peer_random);
    chunk_clear(&this->shared_secret);
    DESTROY_IF(this->peer_id);
    DESTROY_IF(this->my_id);
    free(this);
}
```

---

## 5. 修改范围

| 文件 | 修改内容 |
|------|----------|
| `gmalg_ke.c` | 修改 `get_public_key()`, `set_public_key()`, `get_shared_secret()`, `destroy()` |
| `gmalg_ke.c` | 添加 `set_peer_id()`, `set_my_id()`, `set_role()` |
| `gmalg_ke.c` | 扩展 `private_gmalg_sm2_ke_t` 结构体 |

**不修改任何 strongSwan 核心代码！**

---

## 6. 关键技术细节

### 6.1 随机数生成

必须使用 strongSwan 的 RNG：
```c
rng_t *rng = lib->crypto->create_rng(lib->crypto, RNG_STRONG);
rng->get_bytes(rng, len, buf);
DESTROY_IF(rng);
```

### 6.2 证书用途过滤

必须添加加密用途过滤，避免获取签名证书：
```c
CERT_KEY_USAGE, XKU_KEY_ENCIPHERMENT
```

### 6.3 内存管理

- 使用 `DESTROY_IF()` 安全释放对象
- 使用 `chunk_clear()` 清除敏感数据
- 在 `destroy()` 中释放所有注入的 ID

---

## 7. 测试验证

### 7.1 本地回环测试

```bash
# 启动 charon
sudo /usr/local/libexec/ipsec/charon &

# 加载配置
sudo swanctl --load-all

# 发起连接
sudo swanctl --initiate --child ipsec
```

### 7.2 预期结果

```
[CFG] selected proposal: .../KE1_(1051)
[IKE] PQ-GM-IKEv2: will send certificates in IKE_INTERMEDIATE #0
[ENC] generating IKE_INTERMEDIATE request 1 [ KE ]
[ENC] parsed IKE_INTERMEDIATE response 1 [ KE ]
...
[IKE] IKE_SA established
```

---

## 8. 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 内存泄漏 | 性能测试崩溃 | 使用 DESTROY_IF 和 chunk_clear |
| 证书获取错误 | 握手失败 | 添加 CERT_KEY_USAGE 过滤 |
| 共享秘密不一致 | 认证失败 | 区分角色，保证拼接顺序 |

---

## 9. 参考资料

- strongSwan KE 接口: `src/libstrongswan/crypto/key_exchange.h`
- credential manager: `src/libstrongswan/credentials/credential_manager.h`
- bus 机制: `src/libcharon/bus/bus.h`
- X509 证书: `src/libstrongswan/credentials/certificates/x509.h`
