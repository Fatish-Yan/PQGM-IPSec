# SM2-KEM 真实加密实现设计

**日期**: 2026-02-28
**状态**: 已批准
**优先级**: HIGH

## 背景

当前 5-RTT 实现中，`gmalg_ke.c` 的 SM2-KEM 密钥交换使用 TEST MODE 绕过：
- `get_public_key()` 直接返回 `my_random`（32B），未做 SM2 加密
- `set_public_key()` 直接复制收到的字节，未做 SM2 解密
- 已安装证书使用 ED25519，不是 SM2

本设计修复上述问题，实现完整正确的 SM2 加密/解密。

## 目标

1. `get_public_key()` 使用对端 SM2 公钥加密 `my_random`，密文约 141B
2. `set_public_key()` 使用本端 SM2 私钥解密收到的密文，还原 `peer_random`
3. 安装正确的 SM2 证书（`enc_cert.pem` / `enc_key.pem`）
4. 过滤证书时区分 EncCert（SM2/EC 类型）与 SignCert（ED25519 类型）

## 架构

### 执行流程（修复后）

```
Initiator                                    Responder
─────────────────────────────────────────────────────
get_public_key():                            set_public_key():
  1. 枚举证书，找 SM2(KEY_EC) 类型 EncCert   1. 枚举私钥，找 SM2 私钥
  2. get_encoding(PUBKEY_SPKI_ASN1_DER)       2. get_encoding(PRIVKEY_ASN1_DER)
  3. sm2_public_key_info_from_der()           3. sm2_private_key_info_from_der()
  4. 生成 my_random (32B)                     4. sm2_decrypt_data(ciphertext) → peer_random
  5. sm2_encrypt_data() → ciphertext (~141B)  5. compute_shared_secret()
  6. 返回 ciphertext ──────────────────────►
                                              get_public_key():
                                                同 Initiator 逻辑（生成 r_r）
  set_public_key():          ◄──────────────
    sm2_decrypt_data() → peer_random
    compute_shared_secret()
```

### 关键依赖

与 `gmalg_signer.c:157-167` 相同的密钥加载模式，已验证可用：
```c
// 公钥加载（已在 signer 中验证）
sm2_public_key_info_from_der(&sm2_key, &ptr, &len);

// 私钥加载（已在 signer 中验证）
sm2_private_key_info_from_der(&sm2_key, NULL, NULL, &ptr, &len);
```

## 具体修改

### 前置条件：安装 SM2 证书

将 `/home/ipsec/PQGM-IPSec/certs/` 中的 SM2 证书安装到 swanctl 目录：

```bash
# 每端安装自己的私钥 + 对端的 EncCert
# 回环测试：两端共享同一系统，各自需要 initiator + responder 的证书
cp certs/initiator/enc_cert.pem /usr/local/etc/swanctl/x509/initiator_enc_cert.pem
cp certs/responder/enc_cert.pem /usr/local/etc/swanctl/x509/responder_enc_cert.pem
cp certs/initiator/enc_key.pem  /usr/local/etc/swanctl/private/initiator_enc_key.pem
cp certs/responder/enc_key.pem  /usr/local/etc/swanctl/private/responder_enc_key.pem
```

### 修改 1：`get_public_key()` — SM2 加密

**文件**: `gmalg_ke.c`，约第 481-496 行（TEST MODE 块）

**删除**（TEST MODE bypass）：
```c
/* For now, return my_random as the "ciphertext" for testing */
ciphertext_buf = malloc(SM2_KEM_CIPHERTEXT_SIZE);
ctlen = SM2_KEM_CIPHERTEXT_SIZE;
memcpy(ciphertext_buf, this->my_random.ptr, SM2_KEM_RANDOM_SIZE);
ctlen = SM2_KEM_RANDOM_SIZE;
```

**替换为**（真实 SM2 加密）：
```c
/* Extract DER from public_key_t (same pattern as gmalg_signer.c:167) */
chunk_t key_enc = chunk_empty;
if (!peer_pubkey->get_encoding(peer_pubkey, PUBKEY_SPKI_ASN1_DER, &key_enc))
{
    DBG1(DBG_IKE, "SM2-KEM: failed to get DER encoding of peer pubkey");
    chunk_clear(&this->my_random);
    peer_pubkey->destroy(peer_pubkey);
    return FALSE;
}

/* Parse into SM2_KEY */
SM2_KEY sm2_peer_key;
const uint8_t *ptr = key_enc.ptr;
size_t klen = key_enc.len;
if (sm2_public_key_info_from_der(&sm2_peer_key, &ptr, &klen) != 1)
{
    DBG1(DBG_IKE, "SM2-KEM: failed to parse SM2 public key from DER");
    chunk_free(&key_enc);
    chunk_clear(&this->my_random);
    peer_pubkey->destroy(peer_pubkey);
    return FALSE;
}
chunk_free(&key_enc);

/* Real SM2 encrypt */
ciphertext_buf = malloc(SM2_KEM_CIPHERTEXT_SIZE);
ctlen = SM2_KEM_CIPHERTEXT_SIZE;
if (!sm2_encrypt_data(&sm2_peer_key,
                      this->my_random.ptr, this->my_random.len,
                      ciphertext_buf, &ctlen))
{
    DBG1(DBG_IKE, "SM2-KEM: sm2_encrypt_data failed");
    free(ciphertext_buf);
    chunk_clear(&this->my_random);
    peer_pubkey->destroy(peer_pubkey);
    return FALSE;
}
```

### 修改 2：`set_public_key()` — SM2 解密

**文件**: `gmalg_ke.c`，约第 539-555 行（TEST MODE 块）

**删除**（TEST MODE bypass）：
```c
/* For testing: just copy value as peer_random */
plaintext_buf = malloc(SM2_KEM_RANDOM_SIZE);
ptlen = SM2_KEM_RANDOM_SIZE;
memcpy(plaintext_buf, value.ptr, SM2_KEM_RANDOM_SIZE);
```

**替换为**（真实 SM2 解密）：
```c
/* Extract DER from private_key_t (same pattern as gmalg_signer.c:157) */
chunk_t key_enc = chunk_empty;
if (!my_privkey->get_encoding(my_privkey, PRIVKEY_ASN1_DER, &key_enc))
{
    DBG1(DBG_IKE, "SM2-KEM: failed to get DER encoding of privkey");
    my_privkey->destroy(my_privkey);
    return FALSE;
}

/* Parse into SM2_KEY */
SM2_KEY sm2_my_key;
const uint8_t *ptr = key_enc.ptr;
size_t klen = key_enc.len;
if (sm2_private_key_info_from_der(&sm2_my_key, NULL, NULL, &ptr, &klen) != 1)
{
    DBG1(DBG_IKE, "SM2-KEM: failed to parse SM2 private key from DER");
    chunk_free(&key_enc);
    my_privkey->destroy(my_privkey);
    return FALSE;
}
chunk_free(&key_enc);

/* Real SM2 decrypt */
plaintext_buf = malloc(SM2_KEM_RANDOM_SIZE);
ptlen = SM2_KEM_RANDOM_SIZE;
if (!sm2_decrypt_data(&sm2_my_key,
                      value.ptr, value.len,
                      plaintext_buf, &ptlen))
{
    DBG1(DBG_IKE, "SM2-KEM: sm2_decrypt_data failed");
    free(plaintext_buf);
    my_privkey->destroy(my_privkey);
    return FALSE;
}
```

### 修改 3：证书过滤（区分 EncCert/SignCert）

**文件**: `gmalg_ke.c`，约第 438-448 行（cert 枚举块）

**替换**：在枚举时增加 `KEY_EC` 类型过滤：
```c
certificate_t *cert;
enumerator = lib->credmgr->create_cert_enumerator(lib->credmgr,
    CERT_X509, KEY_EC, NULL, TRUE);   // KEY_EC 匹配 SM2，过滤 ED25519

while (enumerator && enumerator->enumerate(enumerator, &cert))
{
    peer_enc_cert = cert->get_ref(cert);
    break;  // 第一个 SM2(EC) 证书即为 EncCert
}
if (enumerator) enumerator->destroy(enumerator);
```

同理，`set_public_key()` 中私钥查找改为 `KEY_EC`：
```c
my_privkey = lib->credmgr->get_private(lib->credmgr, KEY_EC, NULL, NULL);
```

## 测试验证

### 成功标准

| 验证点 | 预期值 | 验证方法 |
|--------|--------|---------|
| 密文大小 | ~141 bytes | 日志 `returning ciphertext of N bytes` |
| 解密后随机数 | 与对端 `my_random` 前8字节匹配 | 两端 DEBUG 日志对比 |
| IKE_SA 状态 | `ESTABLISHED` | `swanctl --list-sas` |

### 测试步骤

```
1. 安装 SM2 证书到 swanctl
2. make -j$(nproc) && sudo make install（仅 gmalg 插件）
3. 本地回环测试
4. 检查日志密文大小（预期 ~141B）
5. 确认 IKE_SA ESTABLISHED
```

### 回退定位

| 症状 | 可能原因 | 检查方式 |
|------|---------|---------|
| `sm2_encrypt_data failed` | DER 解析失败或 cert 不是 SM2 | 检查 cert 类型 |
| `sm2_decrypt_data failed` | 两端 enc_cert/enc_key 不匹配 | 检查 pubkey 是否对应 privkey |
| `shared secret not available` | ptlen 不等于 SM2_KEM_RANDOM_SIZE | 检查 SM2 明文大小 |

## 文件修改清单

| 文件 | 修改类型 | 行数 |
|------|---------|------|
| `gmalg_ke.c` | 修改 `get_public_key()` | ~20行 |
| `gmalg_ke.c` | 修改 `set_public_key()` | ~20行 |
| `gmalg_ke.c` | 修改 cert/key 枚举过滤 | ~5行 |
| `/usr/local/etc/swanctl/x509/` | 安装 SM2 enc 证书 | 配置 |
| `/usr/local/etc/swanctl/private/` | 安装 SM2 enc 私钥 | 配置 |
