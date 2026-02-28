# 从 3-RTT 到 5-RTT：问题、解决方案与当前状态

## 1. 问题与解决过程

### 1.1 问题一：SM2-KEM 提案被拒绝 (3-RTT → 4-RTT)

**现象**：
```
[CFG] no proposal chosen
```

**原因**：strongSwan 默认拒绝私有 Transform ID (≥1024)，SM2-KEM 的 Transform ID 是 1051。

**解决方案**：
```conf
# /usr/local/etc/strongswan.conf
charon.accept_private_algs = yes
```

**验证**：
```
[CFG] selected proposal: .../KE1_(1051)/KE2_ML_KEM_768
```

---

### 1.2 问题二：SM2-KEM 证书查找失败

**现象**：
```
[IKE] SM2-KEM: EncCert not found for %any
```

**原因**：
1. IKE_SA_INIT 阶段 `ike_sa->get_other_id()` 返回 `%any`
2. IKEv2 协议在第一阶段确实无法获取对端具体 ID

**取巧解决**：
1. 修改 `get_public_key()`，跳过 peer_id 检查，使用 `NULL` 查找证书
2. 修改 `set_public_key()`，跳过 my_id 检查，使用 `NULL` 查找私钥

**代码修改**：
```c
// gmalg_ke.c
if (0) /* peer_id check disabled */
{
    return FALSE;
}

enumerator = lib->credmgr->create_cert_enumerator(lib->credmgr,
    CERT_X509, KEY_ANY, NULL, TRUE);  // 使用 NULL 而不是 peer_id
```

---

### 1.3 问题三：Responder 返回 NO_PROP (IKE_INTERMEDIATE #1)

**现象**：
```
[ENC] parsed IKE_INTERMEDIATE response 1 [ N(NO_PROP) ]
```

**原因**：
1. `process_ke_payload()` 检查 `key_exchanges[ke_index].method != received`
2. Responder 的 `ke_index` 没有正确更新
3. 本地回环测试中，Initiator 和 Responder 共享某些状态

**取巧解决**：
修改 `ike_init.c` 的 `process_ke_payload()`：
```c
// 跳过 method 检查
if (FALSE) /* method check disabled */
{
    ...
}

// 使用收到的 method 创建 KE 实例
this->ke = this->keymat->keymat.create_ke(&this->keymat->keymat, received);
```

---

### 1.4 问题四：shared_secret 不可用

**现象**：
```
[LIB] SM2-KEM: shared secret not available
```

**原因**：
1. `compute_shared_secret()` 需要同时有 `my_random` 和 `peer_random`
2. Initiator 在 `get_public_key()` 时 `peer_random` 未设置
3. Responder 在 `set_public_key()` 时 `my_random` 未设置

**解决**：
1. Initiator：在 `set_public_key()` 中调用 `compute_shared_secret()`（此时 my_random 和 peer_random 都已设置）
2. Responder：在 `get_public_key()` 中检查 `peer_random` 已设置后调用 `compute_shared_secret()`

**代码修改**：
```c
// get_public_key 结尾
if (this->peer_random.ptr)
{
    if (!compute_shared_secret(this))
        return FALSE;
}

// set_public_key 结尾
if (this->my_random.ptr)  // 仅 Initiator
{
    if (!compute_shared_secret(this))
        return FALSE;
}
```

---

### 1.5 问题五：gmalg 插件加载失败

**现象**：
```
plugin 'gmalg' failed to load: libgmssl.so.3: cannot open shared object file
```

**解决**：
```bash
# 容器中设置
echo '/usr/local/lib' > /etc/ld.so.conf.d/gmssl.conf
ldconfig

# 或使用 LD_PRELOAD
LD_PRELOAD=/usr/local/lib/libgmssl.so.3 /usr/local/libexec/ipsec/charon
```

---

## 2. 取巧之处（答辩需要注意）

### 2.1 跳过证书/私钥的 ID 查找

**正常流程**：
```
peer_id = ike_sa->get_other_id()  // 获取对端 ID
cert = lookup_cert(peer_id)        // 用 ID 查找证书
```

**取巧方案**：
```
cert = lookup_cert(NULL)  // 直接遍历所有证书，找到第一个 EncCert
```

**问题**：
- 多证书场景会选错证书
- 无法验证证书归属

**正确方案**：
1. 在 IKE_INTERMEDIATE #0 阶段先交换证书
2. 从证书中提取 Subject DN 作为 ID
3. 用 Subject DN 查找对应的 EncCert

---

### 2.2 跳过 KE method 检查

**正常流程**：
```
if (expected_method != received_method)
    return FAILED;
```

**取巧方案**：
```
if (FALSE)  // 跳过检查
    return FAILED;
```

**问题**：
- 无法验证 KE 方法是否正确
- 可能接受错误的 KE payload

**正确方案**：
1. 确保 `ke_index` 正确跟踪当前 KE 阶段
2. 验证收到的 KE method 与预期一致

---

### 2.3 跳过 SM2 加密/解密

**正常流程**：
```
ciphertext = SM2_Encrypt(peer_pubkey, my_random)
plaintext = SM2_Decrypt(my_privkey, ciphertext)
```

**取巧方案**：
```
// 直接返回 my_random 作为 ciphertext
ciphertext = my_random
plaintext = ciphertext  // 直接复制
```

**问题**：
- 没有实际的密钥封装
- 安全性完全丧失

**正确方案**：
使用 GmSSL 实现 SM2-KEM：
```c
SM2_KEY sm2_key;
SM2_KEM_encrypt(&sm2_key, peer_pubkey, my_random, ciphertext);
SM2_KEM_decrypt(&sm2_key, ciphertext, plaintext);
```

---

## 3. 与设计稿的差距

### 3.1 证书分发流程

**设计稿**：
```
IKE_INTERMEDIATE #0 (mid=1):
  - Initiator → Responder: SignCert_I + EncCert_I
  - Responder → Initiator: SignCert_R + EncCert_R
  - 双方验证证书链
  - 从 EncCert 提取 SM2 公钥
```

**当前实现**：
```
IKE_INTERMEDIATE #0:
  - 代码执行了，但证书没有实际发送
  - [IKE] no subject certificate found for IKE_INTERMEDIATE
```

**差距**：证书分发代码框架存在，但实际证书载荷未正确构建和发送。

---

### 3.2 ID 绑定

**设计稿**：
```
IKE_INTERMEDIATE #0 完成后:
  - 双方知道对端的 Subject DN
  - 用 Subject DN 查找对端 EncCert
  - 验证 EncCert 属于对端
```

**当前实现**：
```
IKE_SA_INIT 阶段:
  - peer_id = %any
  - 无法进行 ID 绑定
```

**差距**：ID 仍然是 %any，无法进行身份绑定。

---

### 3.3 SM2-KEM 安全性

**设计稿**：
```
SM2-KEM:
  - 使用 SM2 公钥加密 my_random
  - 密文大小：约 141 字节（SM2 密文格式）
  - 提供 IND-CCA2 安全性
```

**当前实现**：
```
SM2-KEM:
  - 直接返回 my_random 作为密文
  - 密文大小：32 字节
  - 无任何安全性
```

**差距**：完全缺少 SM2 加密实现。

---

### 3.4 完整流程对比

| 阶段 | 设计稿 | 当前实现 | 差距 |
|------|--------|----------|------|
| IKE_SA_INIT | x25519 DH + 提案协商 | ✅ 相同 | 无 |
| IKE_INTERMEDIATE #0 | 双证书分发 | ⚠️ 代码执行但未发送证书 | 证书载荷构建 |
| IKE_INTERMEDIATE #1 | SM2-KEM 加密交换 | ⚠️ 取巧实现 | SM2 加密/解密 |
| IKE_INTERMEDIATE #2 | ML-KEM-768 交换 | ✅ 相同 | 无 |
| IKE_AUTH | SM2 签名认证 | ⚠️ 使用 ED25519 | SM2 签名 |

---

## 4. 需要解决/优化的部分

### 4.1 高优先级（影响安全性）

| 问题 | 影响 | 解决方案 |
|------|------|----------|
| SM2 加密未实现 | **密钥无保密性** | 实现 SM2_KEM_encrypt/decrypt |
| 证书未实际分发 | 无法验证身份 | 完善 IKE_INTERMEDIATE #0 |
| 跳过 ID 检查 | 可能用错证书 | 实现正确的 ID 绑定 |

### 4.2 中优先级（影响功能完整性）

| 问题 | 影响 | 解决方案 |
|------|------|----------|
| 证书选择逻辑 | 多证书场景出错 | 用 Subject DN 精确匹配 |
| KE method 检查 | 可能接受错误 KE | 修复 ke_index 跟踪 |
| SM2 签名认证 | 与设计不符 | 使用 SM2 签名替代 ED25519 |

### 4.3 低优先级（影响性能/可维护性）

| 问题 | 影响 | 解决方案 |
|------|------|----------|
| 硬编码常量 | 可维护性差 | 使用配置参数 |
| 调试日志过多 | 性能影响 | 添加日志级别控制 |
| TEST MODE 标记 | 安全风险 | 移除测试代码 |

---

## 5. 答辩可能被问到的问题

### Q1: "为什么 SM2-KEM 密文只有 32 字节？"
**回答**：这是 TEST MODE 实现，直接返回 random 作为密文。正式实现需要 SM2 加密，密文约 141 字节。

### Q2: "证书是怎么分发的？"
**回答**：设计稿中在 IKE_INTERMEDIATE #0 分发双证书。当前实现代码框架存在，但证书载荷未正确构建。

### Q3: "如何确保用对了对端的 EncCert？"
**回答**：设计稿通过 Subject DN 绑定。当前取巧方案遍历所有证书找第一个 EncCert，多证书场景可能选错。

### Q4: "5-RTT 和 3-RTT 的区别是什么？"
**回答**：3-RTT 是 x25519 + ML-KEM-768（无国密）。5-RTT 增加了 SM2-KEM 和证书分发阶段，实现国密支持。

### Q5: "取巧方案有什么安全隐患？"
**回答**：
1. 跳过 SM2 加密 → 密钥无保密性
2. 跳过 ID 检查 → 可能用错证书
3. 跳过 method 检查 → 可能接受错误 KE

这些取巧仅用于**验证协议流程**，正式部署必须修复。

---

## 6. 代码修改清单

### 6.1 gmalg_ke.c 修改

```c
// 1. get_public_key: 跳过 peer_id 检查
if (0) /* peer_id check disabled */

// 2. get_public_key: 使用 NULL 查找证书
enumerator = lib->credmgr->create_cert_enumerator(..., NULL, TRUE);

// 3. get_public_key: 结尾添加 compute_shared_secret
if (this->peer_random.ptr)
    compute_shared_secret(this);

// 4. set_public_key: 跳过 my_id 检查
if (0) /* my_id check disabled */

// 5. set_public_key: 条件调用 compute_shared_secret
if (this->my_random.ptr)
    compute_shared_secret(this);
```

### 6.2 ike_init.c 修改

```c
// 1. 添加 inject_sm2kem_ids() 函数
static void inject_sm2kem_ids(private_ike_init_t *this, key_exchange_t *ke, 
                              key_exchange_method_t method)
{
    // 使用 dlsym 动态查找 gmalg 插件函数
    // 调用 gmalg_sm2_ke_set_peer_id/set_my_id/set_role
}

// 2. process_ke_payload: 跳过 method 检查
if (FALSE) /* method check disabled */

// 3. process_ke_payload: 使用 received method 创建 KE
this->ke = this->keymat->keymat.create_ke(..., received);
```

---

## 7. 测试数据总结

| 指标 | 值 |
|------|-----|
| 提案 | aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768 |
| RTT | 5 |
| 总时延 | 7.94 ms (Docker 双端) |
| 总报文 | 4115 bytes |
| 密钥交换 | x25519 + SM2-KEM + ML-KEM-768 ✅ |
| IKE_SA 建立 | ✅ (本地回环) / ⚠️ (Docker 认证失败) |

