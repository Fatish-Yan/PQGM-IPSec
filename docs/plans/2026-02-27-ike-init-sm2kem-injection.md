# IKE 任务代码集成 SM2-KEM ID 注入 - 实现计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 在 IKE 任务代码中调用 SM2-KEM 的 ID 注入接口

**Architecture:** 修改 ike_init.c，在创建 KE 实例后检测 SM2-KEM 并注入 ID

**Tech Stack:** C, strongSwan libcharon

---

## 修改范围

**修改文件：**
- `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c`

---

## Task 1: 添加头文件包含

**Files:**
- Modify: `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c` (文件开头)

**Step 1: 在 #include 部分添加 gmalg_ke.h**

找到现有的 #include 部分（约第 20-40 行），在最后一个 #include 后添加：

```c
#include <encoding/payloads/ke_payload.h>

/* For SM2-KEM ID injection */
#include <plugins/gmalg/gmalg_ke.h>
```

**Step 2: 验证编译**

Run: `cd /home/ipsec/strongswan && make -j$(nproc) 2>&1 | grep -E "error|ike_init" | head -10`
Expected: 无编译错误

**Step 3: Commit**

```bash
git add src/libcharon/sa/ikev2/tasks/ike_init.c
git commit -m "feat(ike): include gmalg_ke.h for SM2-KEM ID injection"
```

---

## Task 2: 添加辅助函数

**Files:**
- Modify: `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c` (在 struct 定义后)

**Step 1: 在 private_ike_init_t 结构体定义后添加辅助函数**

找到 `private_ike_init_t` 结构体定义结束的位置（约第 150-200 行），在其后添加：

```c
/**
 * Inject IDs into SM2-KEM instance if applicable
 */
static void inject_sm2kem_ids(private_ike_init_t *this, key_exchange_t *ke,
							  key_exchange_method_t method)
{
	if (method == KE_SM2)
	{
		identification_t *peer_id, *my_id;

		peer_id = this->ike_sa->get_other_id(this->ike_sa);
		my_id = this->ike_sa->get_my_id(this->ike_sa);

		if (peer_id && my_id)
		{
			DBG1(DBG_IKE, "SM2-KEM: injecting IDs - peer=%Y, my=%Y",
				 peer_id, my_id);
			gmalg_sm2_ke_set_peer_id(ke, peer_id);
			gmalg_sm2_ke_set_my_id(ke, my_id);
			gmalg_sm2_ke_set_role(ke, this->initiator);
		}
		else
		{
			DBG1(DBG_IKE, "SM2-KEM: failed to get IDs for injection");
		}
	}
}
```

**Step 2: 验证编译**

Run: `cd /home/ipsec/strongswan && make -j$(nproc) 2>&1 | grep -E "error" | head -10`
Expected: 无编译错误

**Step 3: Commit**

```bash
git add src/libcharon/sa/ikev2/tasks/ike_init.c
git commit -m "feat(ike): add inject_sm2kem_ids helper function"
```

---

## Task 3: 在位置 1 调用注入函数

**Files:**
- Modify: `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c:637-645`

**Step 1: 在第一个 create_ke 调用后添加注入**

找到第 637 行附近的代码，修改为：

```c
	if (!this->initiator)
	{
		DESTROY_IF(this->ke);
		this->ke = this->keymat->keymat.create_ke(&this->keymat->keymat,
												  method);
		if (!this->ke)
		{
			DBG1(DBG_IKE, "negotiated key exchange method %N not supported",
				 key_exchange_method_names, method);
		}
		else
		{
			/* Inject IDs for SM2-KEM */
			inject_sm2kem_ids(this, this->ke, method);
		}
	}
```

**Step 2: 验证编译**

Run: `cd /home/ipsec/strongswan && make -j$(nproc) 2>&1 | grep -E "error" | head -10`
Expected: 无编译错误

**Step 3: Commit**

```bash
git add src/libcharon/sa/ikev2/tasks/ike_init.c
git commit -m "feat(ike): inject SM2-KEM IDs in process_ke_payload"
```

---

## Task 4: 在位置 2 调用注入函数

**Files:**
- Modify: `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c:818-828`

**Step 1: 在 build_payloads_multi_ke 中的 create_ke 后添加注入**

找到第 818 行附近的代码，修改为：

```c
	message->set_exchange_type(message, exchange_type_multi_ke(this));

	DESTROY_IF(this->ke);
	method = this->key_exchanges[this->ke_index].method;
	this->ke = this->keymat->keymat.create_ke(&this->keymat->keymat,
											  method);
	if (!this->ke)
	{
		DBG1(DBG_IKE, "negotiated key exchange method %N not supported",
			 key_exchange_method_names, method);
		return FAILED;
	}

	/* Inject IDs for SM2-KEM */
	inject_sm2kem_ids(this, this->ke, method);

	if (!build_payloads_multi_ke(this, message))
	{
		return FAILED;
	}
	return NEED_MORE;
```

**Step 2: 验证编译**

Run: `cd /home/ipsec/strongswan && make -j$(nproc) 2>&1 | grep -E "error" | head -10`
Expected: 无编译错误

**Step 3: Commit**

```bash
git add src/libcharon/sa/ikev2/tasks/ike_init.c
git commit -m "feat(ike): inject SM2-KEM IDs in build_payloads_multi_ke"
```

---

## Task 5: 在位置 3 调用注入函数

**Files:**
- Modify: `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c:879-900`

**Step 1: 在 build_i 中的 create_ke 后添加注入**

找到第 879 行附近的代码（build_i 函数中），修改为：

```c
		this->ke = this->keymat->keymat.create_ke(&this->keymat->keymat,
												  this->ke_method);
		if (!this->ke)
		{
			DBG1(DBG_IKE, "configured key exchange method %N not supported",
				 key_exchange_method_names, this->ke_method);
			return FAILED;
		}

		/* Inject IDs for SM2-KEM */
		inject_sm2kem_ids(this, this->ke, this->ke_method);
	}
	else if (this->ke->get_method(this->ke) != this->ke_method)
	{	/* reset KE instance if method changed (INVALID_KE_PAYLOAD) */
		this->ke->destroy(this->ke);
		this->ke = this->keymat->keymat.create_ke(&this->keymat->keymat,
												  this->ke_method);
		if (!this->ke)
		{
			DBG1(DBG_IKE, "requested key exchange method %N not supported",
				 key_exchange_method_names, this->ke_method);
			return FAILED;
		}

		/* Inject IDs for SM2-KEM */
		inject_sm2kem_ids(this, this->ke, this->ke_method);
	}
```

**Step 2: 验证编译**

Run: `cd /home/ipsec/strongswan && make -j$(nproc) 2>&1 | grep -E "error" | head -10`
Expected: 无编译错误

**Step 3: Commit**

```bash
git add src/libcharon/sa/ikev2/tasks/ike_init.c
git commit -m "feat(ike): inject SM2-KEM IDs in build_i"
```

---

## Task 6: 安装并测试

**Files:**
- Test: 本地回环测试

**Step 1: 安装更新的代码**

```bash
cd /home/ipsec/strongswan
echo "1574a" | sudo -S make install 2>&1 | tail -5
```

**Step 2: 重启 charon**

```bash
echo "1574a" | sudo -S pkill charon 2>/dev/null || true
sleep 2
echo "1574a" | sudo -S /usr/local/libexec/ipsec/charon --debug-ike 2 > /tmp/charon_sm2kem.log 2>&1 &
sleep 5
```

**Step 3: 加载配置并测试**

```bash
echo "1574a" | sudo -S swanctl --load-all 2>&1 | grep -E "loaded|failed"
echo "1574a" | sudo -S swanctl --initiate --child ipsec 2>&1 | tee /tmp/sm2kem_inject_test.log
```

**Expected:**
- 看到 `SM2-KEM: injecting IDs` 日志
- 不再看到 `peer_id not set` 错误

**Step 4: 检查日志**

```bash
grep -E "SM2-KEM|injecting|peer_id|my_id" /tmp/charon_sm2kem.log | tail -20
```

**Step 5: Commit**

```bash
git add -A
git commit -m "test(ike): verify SM2-KEM ID injection works"
```

---

## 验收标准

1. 编译无错误
2. 日志显示 `SM2-KEM: injecting IDs`
3. 不再显示 `peer_id not set` 错误
4. SM2-KEM 密钥交换成功完成

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| KE_SM2 未定义 | 编译失败 | 确保 include gmalg_plugin.h |
| get_other_id 返回 NULL | 注入失败 | 添加 NULL 检查 |
| 链接失败 | 运行时错误 | 确保 gmalg 插件加载 |
