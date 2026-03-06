# SM2-KEM 真实加密实现计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将 SM2-KEM 密钥交换从 TEST MODE（32B 明文传输）修复为真实 SM2 加密（~141B 密文），使 5-RTT 协议流程具备真实密钥保密性。

**Architecture:** 修改 `gmalg_ke.c` 的 `get_public_key()` 和 `set_public_key()` 方法，通过 strongSwan 的 `get_encoding()` API 提取 DER 字节，再用 GmSSL 的 `sm2_public_key_info_from_der()` / `sm2_private_key_info_from_der()` 解析为 `SM2_KEY`，最终调用 `sm2_encrypt()` / `sm2_decrypt()`。与 `gmalg_signer.c:157-167` 相同的已验证模式。

**Tech Stack:** C, GmSSL 3.1.1 (`sm2_encrypt/decrypt`, `sm2_*_key_info_from_der`), strongSwan 6.0.4 (`lib->credmgr`, `public_key_t::get_encoding`)

---

## 前置知识

### 文件位置
- **修改目标**: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_ke.c`
- **参考模式**: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_signer.c:157-167`
- **SM2 证书源**: `/home/ipsec/PQGM-IPSec/certs/`
- **swanctl 证书目录**: `/usr/local/etc/swanctl/x509/` 和 `/usr/local/etc/swanctl/private/`

### 关键 API
```c
// 提取公钥 DER (strongSwan)
chunk_t enc = chunk_empty;
peer_pubkey->get_encoding(peer_pubkey, PUBKEY_SPKI_ASN1_DER, &enc);

// 解析为 SM2_KEY (GmSSL, 同 gmalg_signer.c:167)
SM2_KEY sm2_key;
const uint8_t *ptr = enc.ptr; size_t len = enc.len;
sm2_public_key_info_from_der(&sm2_key, &ptr, &len);  // returns 1 on success
chunk_free(&enc);

// 加密 (GmSSL)
uint8_t ct[SM2_KEM_CIPHERTEXT_SIZE]; size_t ctlen = SM2_KEM_CIPHERTEXT_SIZE;
sm2_encrypt(&sm2_key, my_random, 32, ct, &ctlen);  // ctlen ≈ 141 after call

// 私钥同理 (同 gmalg_signer.c:157)
sm2_private_key_info_from_der(&sm2_key, NULL, NULL, &ptr, &len);  // returns 1 on success
sm2_decrypt(&sm2_key, ct, ctlen, plaintext, &ptlen);
```

### 当前证书状态
- `/usr/local/etc/swanctl/x509/encCert.pem` — ED25519（**错误**，需要替换）
- `/home/ipsec/PQGM-IPSec/certs/initiator/enc_cert.pem` — SM2（**正确**，CN=initiator.pqgm-enc）
- `/home/ipsec/PQGM-IPSec/certs/responder/enc_cert.pem` — SM2（**正确**，CN=responder.pqgm-enc）

---

## Task 1: 安装 SM2 证书和私钥

**Files:**
- 修改: `/usr/local/etc/swanctl/x509/encCert.pem`（替换）
- 修改: `/usr/local/etc/swanctl/private/encKey.pem`（替换）

本地回环测试时，initiator 和 responder 共享同一系统，因此需要两端的证书都加载。

**Step 1: 备份现有证书**

```bash
sudo cp /usr/local/etc/swanctl/x509/encCert.pem /usr/local/etc/swanctl/x509/encCert.pem.bak
sudo cp /usr/local/etc/swanctl/private/encKey.pem /usr/local/etc/swanctl/private/encKey.pem.bak 2>/dev/null || true
```

**Step 2: 安装 SM2 证书（两端）**

```bash
# 安装 initiator 的 SM2 EncCert
sudo cp /home/ipsec/PQGM-IPSec/certs/initiator/enc_cert.pem \
        /usr/local/etc/swanctl/x509/initiator_encCert.pem

# 安装 responder 的 SM2 EncCert
sudo cp /home/ipsec/PQGM-IPSec/certs/responder/enc_cert.pem \
        /usr/local/etc/swanctl/x509/responder_encCert.pem

# 替换旧的 encCert.pem（用 initiator 的，本地回环时 Responder 加密用）
sudo cp /home/ipsec/PQGM-IPSec/certs/initiator/enc_cert.pem \
        /usr/local/etc/swanctl/x509/encCert.pem
```

**Step 3: 安装 SM2 私钥**

```bash
# initiator 私钥（Responder 解密 Initiator 发来的密文时用，本地回环共享）
sudo cp /home/ipsec/PQGM-IPSec/certs/initiator/enc_key.pem \
        /usr/local/etc/swanctl/private/encKey.pem

# 验证：检查私钥类型
openssl pkey -in /usr/local/etc/swanctl/private/encKey.pem -noout -text 2>/dev/null | grep "EC Private Key\|ASN1 OID\|SM2"
```

预期输出：
```
EC Private Key: (256 bit)
ASN1 OID: SM2
```

**Step 4: 验证证书类型**

```bash
openssl x509 -in /usr/local/etc/swanctl/x509/encCert.pem -noout -text 2>/dev/null | grep "Public Key Algorithm\|ASN1 OID"
```

预期输出：
```
Public Key Algorithm: id-ecPublicKey
ASN1 OID: SM2
```

---

## Task 2: 修复 cert 枚举——用 KEY_EC 过滤 SM2 证书

**Files:**
- Modify: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_ke.c:438-448`

当前代码用 `KEY_ANY` 找第一张证书，无法区分 SM2(EncCert) 和 ED25519(SignCert)。

**Step 1: 修改 cert 枚举（`get_public_key()` 中）**

找到以下代码（约 437-448 行）：
```c
	/* Find peer's EncCert public key using lib->credmgr */
	enumerator = lib->credmgr->create_cert_enumerator(lib->credmgr,
		CERT_X509, KEY_ANY, NULL, TRUE);

	if (enumerator && enumerator->enumerate(enumerator, &peer_enc_cert))
	{
		peer_enc_cert->get_ref(peer_enc_cert);
	}
	if (enumerator)
	{
		enumerator->destroy(enumerator);
	}
```

替换为：
```c
	/* Find peer's EncCert: look for SM2 (KEY_EC) type certificate */
	enumerator = lib->credmgr->create_cert_enumerator(lib->credmgr,
		CERT_X509, KEY_EC, NULL, TRUE);

	while (enumerator && enumerator->enumerate(enumerator, &peer_enc_cert))
	{
		public_key_t *pub = peer_enc_cert->get_public_key(peer_enc_cert);
		if (pub)
		{
			pub->destroy(pub);
			peer_enc_cert->get_ref(peer_enc_cert);
			break;
		}
		peer_enc_cert = NULL;
	}
	if (enumerator)
	{
		enumerator->destroy(enumerator);
	}
```

**Step 2: 修改私钥查找（`set_public_key()` 中）**

找到约 530-531 行：
```c
	my_privkey = lib->credmgr->get_private(lib->credmgr,
		KEY_ANY, NULL, NULL);
```

替换为：
```c
	my_privkey = lib->credmgr->get_private(lib->credmgr,
		KEY_EC, NULL, NULL);
```

---

## Task 3: 修复 `get_public_key()` — 实现真实 SM2 加密

**Files:**
- Modify: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_ke.c:481-497`

**Step 1: 添加必要的局部变量声明**

在 `get_public_key()` 函数顶部（METHOD 块内，约 420-428 行），在现有变量声明中添加：
```c
	SM2_KEY sm2_peer_key;
	chunk_t key_enc = chunk_empty;
	const uint8_t *key_ptr;
	size_t key_len;
```

完整声明区域应变为（原有的 + 新增的）：
```c
	certificate_t *peer_enc_cert = NULL;
	public_key_t *peer_pubkey = NULL;
	enumerator_t *enumerator;
	chunk_t ciphertext = chunk_empty;
	rng_t *rng;
	uint8_t *ciphertext_buf;
	size_t ctlen;
	SM2_KEY sm2_peer_key;
	chunk_t key_enc = chunk_empty;
	const uint8_t *key_ptr;
	size_t key_len;
```

**Step 2: 替换 TEST MODE 加密块**

找到以下代码（约 481-497 行）：
```c
	/* TODO: Encrypt my_random with peer's public key using SM2 */
	/* For now, return my_random as the "ciphertext" for testing */
	/* In production, this should use sm2_encrypt_data() */

	/* Allocate ciphertext buffer */
	ciphertext_buf = malloc(SM2_KEM_CIPHERTEXT_SIZE);
	ctlen = SM2_KEM_CIPHERTEXT_SIZE;

	/* For testing: just copy my_random as ciphertext */
	memcpy(ciphertext_buf, this->my_random.ptr, SM2_KEM_RANDOM_SIZE);
	ctlen = SM2_KEM_RANDOM_SIZE;

	ciphertext = chunk_create(ciphertext_buf, ctlen);
	*value = chunk_clone(ciphertext);

	free(ciphertext_buf);
	peer_pubkey->destroy(peer_pubkey);
```

替换为：
```c
	/* Extract DER from public_key_t (same pattern as gmalg_signer.c:167) */
	if (!peer_pubkey->get_encoding(peer_pubkey, PUBKEY_SPKI_ASN1_DER, &key_enc))
	{
		DBG1(DBG_IKE, "SM2-KEM: failed to get DER encoding of peer pubkey");
		chunk_clear(&this->my_random);
		peer_pubkey->destroy(peer_pubkey);
		return FALSE;
	}

	/* Parse into SM2_KEY */
	key_ptr = key_enc.ptr;
	key_len = key_enc.len;
	if (sm2_public_key_info_from_der(&sm2_peer_key, &key_ptr, &key_len) != 1)
	{
		DBG1(DBG_IKE, "SM2-KEM: failed to parse SM2 public key from DER");
		chunk_free(&key_enc);
		chunk_clear(&this->my_random);
		peer_pubkey->destroy(peer_pubkey);
		return FALSE;
	}
	chunk_free(&key_enc);
	peer_pubkey->destroy(peer_pubkey);

	/* Real SM2 encryption */
	ciphertext_buf = malloc(SM2_KEM_CIPHERTEXT_SIZE);
	ctlen = SM2_KEM_CIPHERTEXT_SIZE;
	if (sm2_encrypt(&sm2_peer_key,
					this->my_random.ptr, this->my_random.len,
					ciphertext_buf, &ctlen) != 1)
	{
		DBG1(DBG_IKE, "SM2-KEM: sm2_encrypt failed");
		free(ciphertext_buf);
		chunk_clear(&this->my_random);
		return FALSE;
	}

	ciphertext = chunk_create(ciphertext_buf, ctlen);
	*value = chunk_clone(ciphertext);
	free(ciphertext_buf);
```

**Step 3: 确认 `peer_pubkey->destroy()` 不重复调用**

检查替换后 `peer_pubkey->destroy(peer_pubkey)` 只在错误路径调用一次，正常路径已在 SM2 解析后调用（Step 2 中已包含）。

---

## Task 4: 修复 `set_public_key()` — 实现真实 SM2 解密

**Files:**
- Modify: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_ke.c:539-556`

**Step 1: 添加局部变量声明**

在 `set_public_key()` 函数顶部（约 514-519 行），在现有变量声明中添加：
```c
	SM2_KEY sm2_my_key;
	chunk_t key_enc = chunk_empty;
	const uint8_t *key_ptr;
	size_t key_len;
```

完整声明区域变为：
```c
	private_key_t *my_privkey = NULL;
	uint8_t *plaintext_buf;
	size_t ptlen;
	SM2_KEY sm2_my_key;
	chunk_t key_enc = chunk_empty;
	const uint8_t *key_ptr;
	size_t key_len;
```

**Step 2: 替换 TEST MODE 解密块**

找到以下代码（约 539-556 行）：
```c
	/* TODO: Decrypt value (ciphertext) with our private key using SM2 */
	/* For now, just copy value as peer_random for testing */
	/* In production, this should use sm2_decrypt_data() */

	/* Allocate plaintext buffer */
	plaintext_buf = malloc(SM2_KEM_RANDOM_SIZE);
	ptlen = SM2_KEM_RANDOM_SIZE;

	/* For testing: just copy value as plaintext */
	memcpy(plaintext_buf, value.ptr, SM2_KEM_RANDOM_SIZE);
	ptlen = SM2_KEM_RANDOM_SIZE;

	/* Store peer's random contribution */
	this->peer_random = chunk_alloc(SM2_KEM_RANDOM_SIZE);
	memcpy(this->peer_random.ptr, plaintext_buf, ptlen);

	free(plaintext_buf);
	my_privkey->destroy(my_privkey);
```

替换为：
```c
	/* Extract DER from private_key_t (same pattern as gmalg_signer.c:157) */
	if (!my_privkey->get_encoding(my_privkey, PRIVKEY_ASN1_DER, &key_enc))
	{
		DBG1(DBG_IKE, "SM2-KEM: failed to get DER encoding of privkey");
		my_privkey->destroy(my_privkey);
		return FALSE;
	}

	/* Parse into SM2_KEY */
	key_ptr = key_enc.ptr;
	key_len = key_enc.len;
	if (sm2_private_key_info_from_der(&sm2_my_key, NULL, NULL, &key_ptr, &key_len) != 1)
	{
		DBG1(DBG_IKE, "SM2-KEM: failed to parse SM2 private key from DER");
		chunk_free(&key_enc);
		my_privkey->destroy(my_privkey);
		return FALSE;
	}
	chunk_free(&key_enc);
	my_privkey->destroy(my_privkey);

	/* Real SM2 decryption */
	plaintext_buf = malloc(SM2_KEM_RANDOM_SIZE);
	ptlen = SM2_KEM_RANDOM_SIZE;
	if (sm2_decrypt(&sm2_my_key,
					value.ptr, value.len,
					plaintext_buf, &ptlen) != 1)
	{
		DBG1(DBG_IKE, "SM2-KEM: sm2_decrypt failed");
		free(plaintext_buf);
		return FALSE;
	}

	if (ptlen != SM2_KEM_RANDOM_SIZE)
	{
		DBG1(DBG_IKE, "SM2-KEM: unexpected plaintext size %zu (expected %d)",
			 ptlen, SM2_KEM_RANDOM_SIZE);
		free(plaintext_buf);
		return FALSE;
	}

	/* Store peer's random contribution */
	this->peer_random = chunk_alloc(SM2_KEM_RANDOM_SIZE);
	memcpy(this->peer_random.ptr, plaintext_buf, ptlen);
	free(plaintext_buf);
```

---

## Task 5: 编译并安装 gmalg 插件

**Files:**
- Build: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/`

**Step 1: 仅编译 gmalg 插件（不用全量编译）**

```bash
cd /home/ipsec/strongswan/src/libstrongswan/plugins/gmalg
make -j$(nproc) 2>&1 | tail -5
```

预期：无错误，最后几行是 `.la` 文件生成。

若有编译错误，检查：
- 变量重复声明（SM2_KEY, chunk_t 等）
- `peer_pubkey->destroy()` 是否双重调用
- `sm2_encrypt` / `sm2_decrypt` 函数名拼写

**Step 2: 安装插件**

```bash
cd /home/ipsec/strongswan/src/libstrongswan/plugins/gmalg
sudo make install 2>&1 | tail -3
```

预期：`install` 成功，`.so` 文件被复制到 `/usr/local/lib/ipsec/plugins/`。

**Step 3: 验证安装**

```bash
ls -la /usr/local/lib/ipsec/plugins/libstrongswan-gmalg.so
```

预期：文件时间戳为当前时间。

---

## Task 6: 测试——本地回环连接

**Step 1: 停止现有 charon 进程**

```bash
sudo pkill -f charon 2>/dev/null || true
sleep 2
```

**Step 2: 重启 charon**

```bash
sudo /usr/local/libexec/ipsec/charon &
sleep 3
```

**Step 3: 加载配置**

```bash
sudo swanctl --load-all 2>&1
```

预期：无 `failed to load` 错误，看到 `loaded certificate` 类日志。

**Step 4: 发起连接**

```bash
sudo swanctl --initiate --child ipsec 2>&1
```

**Step 5: 检查日志——验证 SM2 加密**

```bash
# 查看 charon 日志（或 journalctl）
sudo journalctl -u strongswan -n 100 --no-pager 2>/dev/null || \
  sudo cat /tmp/charon.log 2>/dev/null | tail -50
```

**成功标志**（按优先级）：

1. 密文大小变为 ~141B（不再是 32B）：
   ```
   SM2-KEM: returning ciphertext of 141 bytes
   ```

2. 解密成功（不报错）：
   ```
   SM2-KEM: decrypted peer_random
   ```

3. IKE_SA 建立：
   ```
   IKE_SA pqgm-ikev2[1] established
   ```

**Step 6: 确认 SA 状态**

```bash
sudo swanctl --list-sas
```

预期：看到 `ESTABLISHED` 状态的 IKE_SA。

---

## Task 7: 失败回退诊断

若 SM2 加密失败，按以下顺序排查：

**症状 A: `sm2_encrypt failed`**

```bash
# 检查 EncCert 是否真的是 SM2 类型
openssl x509 -in /usr/local/etc/swanctl/x509/encCert.pem -noout -text | grep "Public Key Algorithm"
# 预期: id-ecPublicKey  (SM2)
# 若是: ED25519  → Task 1 证书安装未完成
```

**症状 B: `sm2_decrypt failed`**

```bash
# 检查公钥与私钥是否匹配
openssl pkey -in /usr/local/etc/swanctl/private/encKey.pem -pubout 2>/dev/null | openssl pkey -pubin -noout -text | grep pub
openssl x509 -in /usr/local/etc/swanctl/x509/encCert.pem -noout -pubkey | openssl pkey -pubin -noout -text | grep pub
# 两者公钥前缀相同则匹配
```

**症状 C: `failed to parse SM2 public key from DER`**

检查 cert 类型是否被 `KEY_EC` 过滤器匹配，可临时改回 `KEY_ANY` 做对比测试。

---

## Task 8: 提交

```bash
cd /home/ipsec/PQGM-IPSec

git add -p  # 审查 gmalg_ke.c 的改动
git commit -m "feat: 实现 SM2-KEM 真实加密/解密

- get_public_key(): 通过 PUBKEY_SPKI_ASN1_DER + sm2_public_key_info_from_der 加载
  对端 SM2 公钥，调用 sm2_encrypt 加密 my_random（密文 ~141B）
- set_public_key(): 通过 PRIVKEY_ASN1_DER + sm2_private_key_info_from_der 加载
  本端 SM2 私钥，调用 sm2_decrypt 解密 peer_random
- cert/key 查找改用 KEY_EC 过滤，确保只选 SM2 类型

修复 TEST MODE bypass（32B 明文传输），5-RTT 流程具备真实 SM2 密钥保密性"
```
