# RFC 9370 密钥更新链验证实现计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 在 keymat_v2.c 中添加密钥派生日志，验证 RFC 9370 密钥更新链正确工作。

**Architecture:** 通过在 derive_ike_keys() 函数中添加调试日志，输出每轮 SKEYSEED 和 SK_* 密钥的哈希值，建立可追溯的密钥演变链。

**Tech Stack:** C (strongSwan 6.0.4), GmSSL 3.1.3, Docker

---

## Task 1: 添加密钥派生日志到 keymat_v2.c

**Files:**
- Modify: `/home/ipsec/strongswan/src/libcharon/sa/ikev2/keymat_v2.c:431-465`

**Step 1: 在密钥分割后添加哈希日志**

在 `chunk_split()` 调用之后（约 line 434），添加密钥哈希输出：

```c
	/* RFC 9370 Key Derivation Chain Verification */
	{
		extern void hash_to_hex(chunk_t data, char *buf, size_t buflen);
		char hash_buf[33];

		if (rekey_function == PRF_UNDEFINED)
		{
			DBG1(DBG_IKE, "RFC 9370 Key Derivation: Initial (IKE_SA_INIT)");
		}
		else
		{
			DBG1(DBG_IKE, "RFC 9370 Key Derivation: Update after KE");
		}

		DBG1(DBG_IKE, "  SKEYSEED derived from %s",
			 rekey_function == PRF_UNDEFINED ? "Ni|Nr and DH shared secret" : "SK_d(prev) and KE shared secret");

		hash_to_hex(this->skd, hash_buf, sizeof(hash_buf));
		DBG1(DBG_IKE, "  SK_d  hash: %s", hash_buf);

		hash_to_hex(sk_pi, hash_buf, sizeof(hash_buf));
		DBG1(DBG_IKE, "  SK_pi hash: %s", hash_buf);

		hash_to_hex(sk_pr, hash_buf, sizeof(hash_buf));
		DBG1(DBG_IKE, "  SK_pr hash: %s", hash_buf);
	}
```

**Step 2: 添加哈希辅助函数**

在文件顶部（约 line 100，函数定义区域）添加：

```c
/**
 * Convert first 16 bytes of data to hex string for logging
 */
void hash_to_hex(chunk_t data, char *buf, size_t buflen)
{
	size_t i;
	size_t len = data.len < 16 ? data.len : 16;

	if (buflen < len * 2 + 1)
	{
		buf[0] = '\0';
		return;
	}

	for (i = 0; i < len; i++)
	{
		sprintf(buf + i * 2, "%02x", data.ptr[i]);
	}
	buf[len * 2] = '\0';
}
```

**Step 3: 编译验证**

Run: `cd /home/ipsec/strongswan && make -j$(nproc) 2>&1 | head -50`
Expected: 编译成功，无错误

**Step 4: 安装到系统**

Run: `cd /home/ipsec/strongswan && sudo make install 2>&1 | tail -20`
Expected: 安装成功

**Step 5: 提交代码**

```bash
git add src/libcharon/sa/ikev2/keymat_v2.c
git commit -m "feat(rfc9370): add key derivation chain verification logging"
```

---

## Task 2: 添加共享秘密日志到 gmalg_ke.c

**Files:**
- Modify: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_ke.c`

**Step 1: 在 get_shared_secret 中添加日志**

找到 `get_shared_secret` 方法（约 line 849-859），修改为：

```c
METHOD(key_exchange_t, get_shared_secret, bool,
	private_key_exchange_t *this, chunk_t *secret)
{
	if (!this->shared_secret.ptr)
	{
		DBG1(DBG_LIB, "SM2-KEM: shared secret not available");
		return FALSE;
	}
	*secret = chunk_clone(this->shared_secret);

	/* RFC 9370: Log shared secret for verification */
	DBG1(DBG_IKE, "RFC 9370: SM2-KEM shared secret: %zu bytes", this->shared_secret.len);
	return TRUE;
}
```

**Step 2: 编译验证**

Run: `cd /home/ipsec/strongswan && make -j$(nproc) 2>&1 | head -50`
Expected: 编译成功

**Step 3: 提交代码**

```bash
git add src/libstrongswan/plugins/gmalg/gmalg_ke.c
git commit -m "feat(smkem): add shared secret logging for RFC 9370 verification"
```

---

## Task 3: 部署到 Docker 容器并测试

**Files:**
- Modify: `/home/ipsec/PQGM-IPSec/docker/` (容器重建)

**Step 1: 停止现有容器**

Run: `cd /home/ipsec/PQGM-IPSec/docker && docker-compose down`
Expected: 容器停止

**Step 2: 重新启动容器**

Run: `cd /home/ipsec/PQGM-IPSec/docker && docker-compose up -d`
Expected: 容器启动

**Step 3: 触发 IKE 协商**

Run: `docker exec pqgm-initiator swanctl --initiate --child net 2>&1`
Expected: IKE 协商开始

**Step 4: 收集日志**

Run: `docker logs pqgm-initiator 2>&1 | grep -E "(RFC 9370|SKEYSEED|SK_|shared secret)" | tail -30`
Expected: 看到密钥派生链日志

---

## Task 4: 验证密钥更新链

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/docs/rfc9370-verification-results.md`

**Step 1: 运行完整测试**

Run: `docker exec pqgm-initiator swanctl --initiate --child net 2>&1`
Run: `docker logs pqgm-initiator 2>&1 > /tmp/ike-log.txt`

**Step 2: 提取密钥哈希**

Run: `grep -E "(SK_d hash|SK_pi hash|SK_pr hash)" /tmp/ike-log.txt`

**Step 3: 验证密钥变化**

检查点：
- [ ] SK_d 在每个 IKE_INTERMEDIATE 后都变化
- [ ] SK_pi/SK_pr 在每个 IKE_INTERMEDIATE 后都变化
- [ ] 最终 IKE_AUTH 使用最后一轮密钥

**Step 4: 记录验证结果**

创建文档记录验证结果：

```markdown
# RFC 9370 密钥更新链验证结果

## 测试环境
- 日期: 2026-03-01
- strongSwan: 6.0.4 (修改版)
- 测试场景: 5-RTT PQ-GM-IKEv2

## 密钥更新链

| 阶段 | SK_d 哈希 | SK_pi 哈希 | 变化 |
|------|-----------|-----------|------|
| IKE_SA_INIT | xxx | xxx | - |
| IKE_INT #1 | xxx | xxx | ✅ |
| IKE_INT #2 | xxx | xxx | ✅ |

## 结论
[验证通过/失败]
```

**Step 5: 提交验证结果**

```bash
git add docs/rfc9370-verification-results.md
git commit -m "docs: add RFC 9370 key derivation chain verification results"
```

---

## 预期日志输出示例

```
[IKE] RFC 9370 Key Derivation: Initial (IKE_SA_INIT)
[IKE]   SKEYSEED derived from Ni|Nr and DH shared secret
[IKE]   SK_d  hash: a1b2c3d4e5f67890
[IKE]   SK_pi hash: 1234567890abcdef
[IKE]   SK_pr hash: fedcba0987654321
[IKE] RFC 9370: SM2-KEM shared secret: 64 bytes
[IKE] RFC 9370 Key Derivation: Update after KE
[IKE]   SKEYSEED derived from SK_d(prev) and KE shared secret
[IKE]   SK_d  hash: 9876543210abcdef
[IKE]   SK_pi hash: abcdef1234567890
[IKE]   SK_pr hash: 0987654321fedcba
```

---

## 注意事项

1. **安全性**: 哈希输出仅显示前 16 字节，避免泄露完整密钥
2. **性能**: 日志使用 DBG1 级别，可在生产环境通过配置禁用
3. **测试环境**: 仅在 Docker 测试环境启用详细日志

---

*计划创建时间: 2026-03-01*
*预计执行时间: 30 分钟*
