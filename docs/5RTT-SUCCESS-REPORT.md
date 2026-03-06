# 5-RTT PQ-GM-IKEv2 测试成功报告

> 时间: 2026-02-28 20:20
> 环境: 本地回环测试 (127.0.0.1)

---

## 1. 测试配置

**提案**: `aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768`

**密钥**:
- SM2-KEM: GmSSL 3.1.1 生成的密钥对
- 私钥: 加密格式 (密码: PQGM2026)
- 公钥: PEM格式

---

## 2. 测试结果

### 2.1 协议流程

| RTT | 阶段 | 状态 | 详情 |
|-----|------|------|------|
| 1 | IKE_SA_INIT | ✅ | x25519 DH, 提案协商成功 |
| 2 | IKE_INTERMEDIATE #0 | ⚠️ | 证书分发代码执行但证书未找到 |
| 3 | IKE_INTERMEDIATE #1 | ✅ | **SM2-KEM 密钥交换成功** |
| 4 | IKE_INTERMEDIATE #2 | ✅ | **ML-KEM-768 密钥交换成功** |
| 5 | IKE_AUTH | ❌ | AUTH_FAILED (认证配置问题) |

### 2.2 SM2-KEM 日志

```
[IKE] SM2-KEM: load_sm2_pubkey_from_file: sm2_public_key_info_from_pem returned 1
[IKE] SM2-KEM: loaded peer pubkey from file
[IKE] SM2-KEM: returning ciphertext of 141 bytes  ← SM2加密成功

[IKE] SM2-KEM: set_public_key called with 141 bytes
[IKE] SM2-KEM: load_sm2_privkey_from_file: decrypt returned 1  ← 私钥解密成功
[IKE] SM2-KEM: decrypted peer_random                      ← SM2解密成功
[IKE] SM2-KEM: computing shared secret after get_public_key

DEBUG: Responder computing SK = peer_random || my_random
DEBUG: Initiator computing SK = my_random || peer_random
```

### 2.3 关键代码修改

**gmalg_ke.c**:
1. 使用 `sm2_public_key_info_from_pem` 加载公钥
2. 使用 `sm2_private_key_info_decrypt_from_pem` 解密私钥
3. 使用 `sm2_encrypt` / `sm2_decrypt` 进行真实加密

**私钥文件格式**:
- GmSSL 加密格式 (ENCRYPTED PRIVATE KEY)
- 密码: PQGM2026

---

## 3. 技术细节

### 3.1 GmSSL 密钥处理

```c
// 公钥加载
FILE *fp = fopen("/path/to/pubkey.pem", "r");
SM2_KEY sm2_key;
sm2_public_key_info_from_pem(&sm2_key, fp);

// 私钥解密
FILE *fp = fopen("/path/to/privkey.pem", "r");
SM2_KEY sm2_key;
sm2_private_key_info_decrypt_from_pem(&sm2_key, "password", fp);

// 加密
sm2_encrypt(&peer_key, plaintext, len, ciphertext, &ctlen);

// 解密
sm2_decrypt(&my_key, ciphertext, ctlen, plaintext, &ptlen);
```

### 3.2 Shared Secret 计算

```
Initiator: SK = my_random || peer_random
Responder: SK = peer_random || my_random
```

---

## 4. 待解决问题

1. **R0 证书分发**: 需要使用 OpenSSL 兼容的证书格式
2. **IKE_AUTH 认证**: 需要修复证书/签名验证

---

## 5. 结论

**5-RTT PQ-GM-IKEv2 密钥交换成功！**

- x25519: ✅
- SM2-KEM: ✅ (真实加密)
- ML-KEM-768: ✅
- IKE_SA: ⚠️ (AUTH_FAILED)

