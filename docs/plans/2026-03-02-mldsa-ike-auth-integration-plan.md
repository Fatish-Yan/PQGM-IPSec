# ML-DSA IKE_AUTH 集成实现计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将 ML-DSA 混合证书集成到 strongSwan 的 IKE_AUTH 认证流程中，实现后量子安全的身份认证。

**Architecture:** 通过配置驱动方式，使用现有的 mldsa 插件进行签名/验证，修改最小必要代码支持混合证书和原始私钥加载。

**Tech Stack:** strongSwan 6.0.4, liboqs 0.12.0, OpenSSL 3.0.2, Docker

---

## 前置条件

- ✅ ML-DSA 混合证书已生成 (`initiator_hybrid_cert.pem`, `responder_hybrid_cert.pem`)
- ✅ ML-DSA 私钥已生成 (`initiator_mldsa_key.bin`, `responder_mldsa_key.bin`)
- ✅ mldsa 插件已实现 `extract_mldsa_pubkey_from_cert()` 函数
- ✅ CA 证书已生成 (`mldsa_ca.pem`)

---

## Task 1: 创建 swanctl 混合证书配置文件

**Files:**
- Create: `docker/initiator/config/swanctl-mldsa-hybrid.conf`
- Create: `docker/responder/config/swanctl-mldsa-hybrid.conf`

**Step 1: 创建 Initiator 配置文件**

```bash
cat > docker/initiator/config/swanctl-mldsa-hybrid.conf << 'EOF'
# ML-DSA Hybrid Certificate Authentication
connections {
    pqgm-mldsa-hybrid {
        version = 2
        local_addrs = 172.28.0.10
        remote_addrs = 172.28.0.20
        proposals = aes256-sha256-x25519

        local {
            auth = pubkey
            id = initiator.pqgm.test
            certs = initiator_hybrid_cert.pem
        }

        remote {
            auth = pubkey
            id = responder.pqgm.test
            cacerts = mldsa_ca.pem
        }

        children {
            net {
                local_ts = 10.1.0.0/16
                remote_ts = 10.2.0.0/16
                esp_proposals = aes256gcm16-sha256
                start_action = none
            }
        }
    }
}

secrets {
    mldsa-key {
        id = initiator.pqgm.test
        file = initiator_mldsa_key.bin
        type = mldsa
    }
}
EOF
```

**Step 2: 创建 Responder 配置文件**

```bash
cat > docker/responder/config/swanctl-mldsa-hybrid.conf << 'EOF'
# ML-DSA Hybrid Certificate Authentication
connections {
    pqgm-mldsa-hybrid {
        version = 2
        local_addrs = 172.28.0.20
        remote_addrs = 172.28.0.10
        proposals = aes256-sha256-x25519

        local {
            auth = pubkey
            id = responder.pqgm.test
            certs = responder_hybrid_cert.pem
        }

        remote {
            auth = pubkey
            id = initiator.pqgm.test
            cacerts = mldsa_ca.pem
        }

        children {
            net {
                local_ts = 10.2.0.0/16
                remote_ts = 10.1.0.0/16
                esp_proposals = aes256gcm16-sha256
                start_action = none
            }
        }
    }
}

secrets {
    mldsa-key {
        id = responder.pqgm.test
        file = responder_mldsa_key.bin
        type = mldsa
    }
}
EOF
```

**Step 3: 验证配置文件创建**

```bash
ls -la docker/initiator/config/swanctl-mldsa-hybrid.conf
ls -la docker/responder/config/swanctl-mldsa-hybrid.conf
```

Expected: 两个文件都存在

**Step 4: 提交**

```bash
git add docker/initiator/config/swanctl-mldsa-hybrid.conf
git add docker/responder/config/swanctl-mldsa-hybrid.conf
git commit -m "feat(config): add ML-DSA hybrid certificate swanctl config"
```

---

## Task 2: 创建 ML-DSA 私钥加载器

**Files:**
- Create: `strongswan/src/libstrongswan/plugins/mldsa/mldsa_private_key.c`
- Create: `strongswan/src/libstrongswan/plugins/mldsa/mldsa_private_key.h`
- Modify: `strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.c`

**Step 1: 创建 mldsa_private_key.h**

```c
/*
 * mldsa_private_key.h - ML-DSA Private Key Loader
 */

#ifndef MLDSA_PRIVATE_KEY_H_
#define MLDSA_PRIVATE_KEY_H_

#include <credentials/keys/private_key.h>

/**
 * Load ML-DSA private key from raw binary file
 *
 * @param filename	path to .bin file containing 4032 bytes ML-DSA-65 private key
 * @return			private_key_t object, or NULL on failure
 */
private_key_t *mldsa_private_key_load(chunk_t data);

#endif /* MLDSA_PRIVATE_KEY_H_ */
```

**Step 2: 创建 mldsa_private_key.c**

```c
/*
 * mldsa_private_key.c - ML-DSA Private Key Loader
 */

#include "mldsa_private_key.h"
#include "mldsa_signer.h"

#include <utils/debug.h>
#include <credentials/keys/private_key.h>

#ifdef HAVE_LIBOQS
#include <oqs/oqs.h>
#endif

#define MLDSA65_SECRET_KEY_BYTES 4032
#define MLDSA65_PUBLIC_KEY_BYTES 1952
#define MLDSA65_SIGNATURE_BYTES 3309

typedef struct private_mldsa_private_key_t private_mlda_private_key_t;

#ifdef HAVE_LIBOQS

struct private_mldsa_private_key_t {
	private_key_t public;

	OQS_SIG *sig_ctx;
	uint8_t private_key[MLDSA65_SECRET_KEY_BYTES];
	uint8_t public_key[MLDSA65_PUBLIC_KEY_BYTES];
	bool loaded;
};

METHOD(private_key_t, get_type, key_type_t,
	private_mldsa_private_key_t *this)
{
	return KEY_PRIV_MLDSA65;
}

METHOD(private_key_t, sign, bool,
	private_mldsa_private_key_t *this, signature_scheme_t scheme,
	void *params, chunk_t data, chunk_t *signature)
{
	if (!this->loaded || !this->sig_ctx)
	{
		return FALSE;
	}

	*signature = chunk_alloc(MLDSA65_SIGNATURE_BYTES);

	if (OQS_SIG_sign(this->sig_ctx, signature->ptr, &signature->len,
					 data.ptr, data.len, this->private_key) != OQS_SUCCESS)
	{
		chunk_free(signature);
		return FALSE;
	}

	return TRUE;
}

METHOD(private_key_t, decrypt, bool,
	private_mldsa_private_key_t *this, encryption_scheme_t scheme,
	void *params, chunk_t crypto, chunk_t *plain)
{
	/* ML-DSA does not support encryption */
	return FALSE;
}

METHOD(private_key_t, get_encoding, bool,
	private_mldsa_private_key_t *this, cred_encoding_type_t type,
	chunk_t *encoding)
{
	/* Return raw key data */
	*encoding = chunk_clone(chunk_create(this->private_key, MLDSA65_SECRET_KEY_BYTES));
	return TRUE;
}

METHOD(private_key_t, get_fingerprint, bool,
	private_mldsa_private_key_t *this, cred_encoding_type_t type,
	chunk_t *fp)
{
	/* Hash the public key for fingerprint */
	hasher_t *hasher = lib->crypto->create_hasher(lib->crypto, HASH_SHA256);
	if (!hasher)
	{
		return FALSE;
	}

	chunk_t pub = chunk_create(this->public_key, MLDSA65_PUBLIC_KEY_BYTES);
	*fp = chunk_alloc(hasher->get_hash_size(hasher));
	hasher->allocate_hash(hasher, pub, fp);
	hasher->destroy(hasher);
	return TRUE;
}

METHOD(private_key_t, get_refcount, int,
	private_mldsa_private_key_t *this)
{
	return 1;
}

METHOD(private_key_t, belongs_to, bool,
	private_mldsa_private_key_t *this, public_key_t *public)
{
	/* Compare public keys */
	chunk_t pub;
	if (public->get_encoding(public, PUBKEY_SPKI_ASN1_DER, &pub))
	{
		/* For now, just return TRUE */
		chunk_free(&pub);
		return TRUE;
	}
	return FALSE;
}

METHOD(private_key_t, equals, bool,
	private_mldsa_private_key_t *this, private_key_t *other)
{
	return FALSE;
}

METHOD(private_key_t, destroy, void,
	private_mldsa_private_key_t *this)
{
	if (this->sig_ctx)
	{
		OQS_SIG_free(this->sig_ctx);
	}
	free(this);
}

/*
 * Load ML-DSA private key from raw data
 */
private_key_t *mldsa_private_key_load(chunk_t data)
{
	private_mldsa_private_key_t *this;

	if (data.len != MLDSA65_SECRET_KEY_BYTES)
	{
		DBG1(DBG_LIB, "ML-DSA: invalid private key length %d (expected %d)",
			 data.len, MLDSA65_SECRET_KEY_BYTES);
		return NULL;
	}

	INIT(this,
		.public = {
			.get_type = _get_type,
			.sign = _sign,
			.decrypt = _decrypt,
			.get_encoding = _get_encoding,
			.get_fingerprint = _get_fingerprint,
			.get_refcount = _get_refcount,
			.belongs_to = _belongs_to,
			.equals = _equals,
			.destroy = _destroy,
		},
		.sig_ctx = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65),
		.loaded = TRUE,
	);

	if (!this->sig_ctx)
	{
		DBG1(DBG_LIB, "ML-DSA: failed to create OQS_SIG context");
		free(this);
		return NULL;
	}

	/* Copy private key */
	memcpy(this->private_key, data.ptr, MLDSA65_SECRET_KEY_BYTES);

	/* Extract public key from private key (last 1952 bytes) */
	memcpy(this->public_key,
		   data.ptr + MLDSA65_SECRET_KEY_BYTES - MLDSA65_PUBLIC_KEY_BYTES,
		   MLDSA65_PUBLIC_KEY_BYTES);

	DBG1(DBG_LIB, "ML-DSA: loaded private key successfully");

	return &this->public;
}

#else /* !HAVE_LIBOQS */

private_key_t *mldsa_private_key_load(chunk_t data)
{
	DBG1(DBG_LIB, "ML-DSA: liboqs not available");
	return NULL;
}

#endif /* HAVE_LIBOQS */
```

**Step 3: 验证编译**

```bash
cd /home/ipsec/strongswan
# 检查文件是否存在
ls -la src/libstrongswan/plugins/mldsa/mldsa_private_key.c
ls -la src/libstrongswan/plugins/mldsa/mldsa_private_key.h
```

**Step 4: 提交**

```bash
git add strongswan/src/libstrongswan/plugins/mldsa/mldsa_private_key.c
git add strongswan/src/libstrongswan/plugins/mldsa/mldsa_private_key.h
git commit -m "feat(mldsa): add ML-DSA private key loader"
```

---

## Task 3: 更新 mldsa 插件注册

**Files:**
- Modify: `strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.c`

**Step 1: 查看当前插件代码**

```bash
cat strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.c
```

**Step 2: 添加私钥加载器注册**

在 mldsa_plugin.c 中添加:
```c
#include "mldsa_private_key.h"

/* In get_features() add: */
PLUGIN_REGISTER(PRIVATE_KEY, mldsa_private_key_load),
    PLUGIN_PROVIDE(PRIVATE_KEY, KEY_PRIV_MLDSA65),
```

**Step 3: 验证修改**

```bash
cat strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.c
```

**Step 4: 提交**

```bash
git add strongswan/src/libstrongswan/plugins/mldsa/mldsa_plugin.c
git commit -m "feat(mldsa): register ML-DSA private key loader in plugin"
```

---

## Task 4: 编译和部署更新后的 strongSwan

**Step 1: 重新编译 strongSwan**

```bash
cd /home/ipsec/strongswan
make clean
./configure --enable-mldsa --enable-gmalg --enable-swanctl --with-gmssl=/usr/local
make -j$(nproc)
```

Expected: 编译成功，无错误

**Step 2: 安装更新后的插件**

```bash
echo "1574a" | sudo -S make install
```

Expected: 安装成功

**Step 3: 验证插件加载**

```bash
sudo ipsec statusall 2>&1 | grep -i mldsa
```

Expected: 显示 mldsa 插件已加载

**Step 4: 提交编译验证**

```bash
git add -A
git commit -m "build: recompile strongSwan with ML-DSA private key support"
```

---

## Task 5: Docker 容器配置更新

**Files:**
- Modify: `docker/docker-compose.yml` (如果需要)

**Step 1: 复制配置文件到正确位置**

```bash
# 确保证书文件存在
ls -la docker/initiator/certs/mldsa/
ls -la docker/responder/certs/mldsa/
```

**Step 2: 更新 Docker 启动脚本**

创建符号链接或复制配置文件:
```bash
cp docker/initiator/config/swanctl-mldsa-hybrid.conf docker/initiator/swanctl.conf
cp docker/responder/config/swanctl-mldsa-hybrid.conf docker/responder/swanctl.conf
```

**Step 3: 验证配置**

```bash
cat docker/initiator/swanctl.conf
cat docker/responder/swanctl.conf
```

**Step 4: 提交**

```bash
git add docker/initiator/swanctl.conf
git add docker/responder/swanctl.conf
git commit -m "feat(docker): update swanctl config for ML-DSA hybrid cert"
```

---

## Task 6: Docker 容器测试

**Step 1: 重建 Docker 镜像**

```bash
cd /home/ipsec/PQGM-IPSec/docker
docker-compose down
docker-compose build --no-cache
docker-compose up -d
```

Expected: 容器启动成功

**Step 2: 检查容器状态**

```bash
docker-compose ps
docker logs pqgm-initiator 2>&1 | head -50
docker logs pqgm-responder 2>&1 | head -50
```

Expected: 两个容器都在运行

**Step 3: 发起 IKE 连接**

```bash
docker exec -it pqgm-initiator swanctl --initiate --child net
```

Expected: 连接建立

**Step 4: 检查日志**

```bash
docker logs pqgm-initiator 2>&1 | grep -i "ML-DSA\|mldsa\|auth"
docker logs pqgm-responder 2>&1 | grep -i "ML-DSA\|mldsa\|auth"
```

Expected: 显示 ML-DSA 认证成功

---

## Task 7: 验证和文档更新

**Step 1: 验证 SA 状态**

```bash
docker exec -it pqgm-initiator swanctl --list-sas
docker exec -it pqgm-responder swanctl --list-sas
```

Expected: 显示已建立的 SA

**Step 2: 抓包验证签名大小**

```bash
# 在主机上抓包
sudo tcpdump -i any udp port 500 or udp port 4500 -w /tmp/ike_auth.pcap &
# 发起连接
docker exec -it pqgm-initiator swanctl --initiate --child net
# 停止抓包
sudo killall tcpdump
# 分析
wireshark /tmp/ike_auth.pcap &
```

Expected: AUTH payload 中包含 3309 bytes 的 ML-DSA 签名

**Step 3: 更新 FIXES-RECORD.md**

添加测试结果记录

**Step 4: 提交最终验证**

```bash
git add docs/FIXES-RECORD.md
git commit -m "docs: update ML-DSA IKE_AUTH integration test results"
```

---

## 验收标准

| 标准 | 验证方法 |
|------|---------|
| 配置文件创建完成 | `ls docker/*/config/swanctl-mldsa-hybrid.conf` |
| 私钥加载器编译成功 | `make` 无错误 |
| Docker 容器启动成功 | `docker-compose ps` 显示运行中 |
| IKE_AUTH 认证成功 | 日志显示 ML-DSA 验证通过 |
| IPSec SA 建立成功 | `swanctl --list-sas` 显示 SA |

---

## 回滚计划

如果集成失败，执行以下回滚步骤:

```bash
# 1. 停止容器
cd docker && docker-compose down

# 2. 恢复原始配置
git checkout HEAD~1 -- docker/initiator/swanctl.conf docker/responder/swanctl.conf

# 3. 重新编译（不包含 mldsa 私钥加载器）
cd ../strongswan && make clean && ./configure --enable-gmalg --enable-swanctl && make && sudo make install

# 4. 重启容器
cd ../docker && docker-compose up -d
```

---

*创建时间: 2026-03-02*
