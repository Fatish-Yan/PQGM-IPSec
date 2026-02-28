# 当前项目问题与解决方案

## 问题总览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        PQ-GM-IKEv2 当前问题分类                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  高优先级 (安全性)                                                       │
│  ├── 🔴 P1: SM2-KEM 无真实加密 (密钥无保密性)                            │
│  ├── 🔴 P2: SM2 证书无法被 OpenSSL 解析                                  │
│  └── 🔴 P3: SM2 私钥格式不兼容                                          │
│                                                                         │
│  中优先级 (功能完整性)                                                   │
│  ├── 🟡 P4: 证书未实际分发 (IKE_INTERMEDIATE #0)                        │
│  ├── 🟡 P5: ID 绑定缺失 (仍是 %any)                                      │
│  └── 🟡 P6: KE method 检查被跳过                                        │
│                                                                         │
│  低优先级 (代码质量)                                                     │
│  ├── 🟢 P7: TEST MODE 代码残留                                          │
│  └── 🟢 P8: 硬编码常量                                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 高优先级问题详解

### 🔴 P1: SM2-KEM 无真实加密

**问题描述**：
当前 `get_public_key()` 直接返回 `my_random` 作为密文，没有任何加密。

**当前代码**：
```c
// TEST MODE: 直接复制 my_random 作为密文
memcpy(ciphertext_buf, this->my_random.ptr, SM2_KEM_RANDOM_SIZE);
*value = chunk_clone(chunk_create(ciphertext_buf, SM2_KEM_RANDOM_SIZE));
```

**安全问题**：
- 密文 = 明文，完全没有保密性
- 任何监听者都能获取密钥材料

**解决思路 A: 使用 GmSSL SM2 加密（已尝试但被 P2/P3 阻塞）**

```c
SM2_KEY sm2_peer_key;
sm2_encrypt(&sm2_peer_key, 
    this->my_random.ptr, this->my_random.len,
    ciphertext_buf, &ctlen);
```

**阻塞原因**：无法获取对端 SM2 公钥（P2: OpenSSL 不解析 SM2 证书）

**解决思路 B: 使用未加密的密钥文件回退**

```bash
# 生成未加密的密钥
gmssl sm2 -genkey -out peer_sm2_privkey.pem
gmssl sm2 -pubkey -in peer_sm2_privkey.pem -out peer_sm2_pubkey.pem
```

```c
// 从文件加载公钥
SM2_KEY sm2_peer_key;
FILE *fp = fopen("/usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem", "r");
SM2_PUBLIC_KEY_from_pem(&sm2_peer_key, fp);
```

**优点**：
- 工作量低
- 绕过证书解析问题

**缺点**：
- 需要手动管理密钥文件
- 不符合证书体系

**解决思路 C: 使用标准 EC 证书（推荐）**

1. 生成 ECDSA-P256 证书（OpenSSL 支持）
2. 从证书提取 EC 公钥
3. 使用 ECIES 进行密钥封装

```bash
# 生成 ECDSA 证书
openssl ecparam -genkey -name prime256v1 -out ec_key.pem
openssl req -new -x509 -key ec_key.pem -out ec_cert.pem -days 365
```

---

### 🔴 P2: SM2 证书无法被 OpenSSL 解析

**问题描述**：
```
[ASN] unable to parse signature algorithm
[LIB] OpenSSL X.509 parsing failed
```

**原因**：
- strongSwan 使用 OpenSSL 解析 X.509
- SM2-with-SM3 的 OID (1.2.156.10197.1.501) 不在 OpenSSL 默认支持列表

**解决思路 A: 使用 GmSSL 解析后注入（复杂）**

1. 在 gmalg 插件中添加 X.509 解析
2. 使用 GmSSL 解析 SM2 证书
3. 将解析结果注入 strongSwan credential manager

**解决思路 B: 修改证书为 ECDSA（简单）**

1. 用 OpenSSL 生成 ECDSA-P256 证书
2. strongSwan 可以正常解析
3. SM2-KEM 使用单独的密钥文件

**解决思路 C: 贡献 SM2 OID 到 strongSwan（长期）**

向 strongSwan 社区贡献 SM2 OID 支持

---

### 🔴 P3: SM2 私钥加密格式不兼容

**问题描述**：
```
-----BEGIN ENCRYPTED PRIVATE KEY-----
```
私钥使用 SM4-PBKDF2 加密，OpenSSL/strongSwan 无法解析。

**解决思路：生成未加密的私钥**

```bash
# GmSSL 生成未加密私钥
gmssl sm2 -genkey -out sm2_privkey.pem
# 不使用 -pass 参数
```

---

## 中优先级问题详解

### 🟡 P4: 证书未实际分发

**问题**：IKE_INTERMEDIATE #0 代码执行但证书没有实际发送

```
[IKE] no subject certificate found for IKE_INTERMEDIATE
```

**解决思路**：

1. 检查 `ike_cert_pre.c` 的证书载荷构建
2. 确保证书被正确序列化
3. 验证载荷类型和长度

---

### 🟡 P5: ID 绑定缺失

**问题**：`ike_sa->get_other_id()` 返回 `%any`

**解决思路**：

1. 在 IKE_INTERMEDIATE #0 后，从证书提取 Subject DN
2. 将 Subject DN 设置为对端 ID
3. 用 Subject DN 查找 EncCert

---

### 🟡 P6: KE method 检查被跳过

**问题**：
```c
if (FALSE) /* method check disabled */
```

**解决思路**：

修复 `ke_index` 跟踪逻辑，而不是跳过检查。

---

## 推荐解决方案

### 短期方案（论文答辩）

**选项 A: 使用未加密密钥文件**

```bash
# 1. 生成 SM2 密钥对（未加密）
gmssl sm2 -genkey -out /usr/local/etc/swanctl/private/sm2_key.pem

# 2. 导出公钥
gmssl sm2 -pubkey -in /usr/local/etc/swanctl/private/sm2_key.pem \
    -out /usr/local/etc/swanctl/x509/sm2_pubkey.pem

# 3. 修改 gmalg_ke.c 从文件加载
```

**工作量**：1-2 小时

**选项 B: 使用 ECDSA 证书 + SM2 密钥文件**

1. 证书用 ECDSA-P256（OpenSSL 支持）
2. SM2-KEM 用单独的密钥文件

**工作量**：2-4 小时

### 长期方案（生产就绪）

**向 strongSwan 贡献 SM2 支持**

1. 添加 SM2 OID 到 strongSwan
2. 添加 SM2 证书解析
3. 添加 SM2 私钥解析

**工作量**：1-2 周

---

## 决策矩阵

| 方案 | 工作量 | 安全性 | 论文可用 | 生产可用 |
|------|--------|--------|----------|----------|
| 保持 TEST MODE | 0 | ❌ 无 | ⚠️ 需说明 | ❌ |
| 未加密密钥文件 | 2h | ✅ 有 | ✅ | ⚠️ |
| ECDSA 证书 + SM2 | 4h | ✅ 有 | ✅ | ⚠️ |
| 贡献 SM2 支持 | 2w | ✅ 有 | ✅ | ✅ |

---

## 答辩准备

### 可能的问题

**Q: "当前 SM2-KEM 有加密吗？"**

A: 当前是 TEST MODE，密文 = 明文。已实现调用框架，但被证书解析问题阻塞。

**Q: "为什么证书无法加载？"**

A: SM2-with-SM3 签名算法 OID 不在 OpenSSL 默认支持中。解决方案是用 GmSSL 生成未加密密钥，从文件加载。

**Q: "如何确保安全？"**

A: TEST MODE 仅用于验证协议流程。正式部署需要：
1. 使用 GmSSL 进行真实 SM2 加密
2. 或使用 ECDSA 证书 + ECIES

**Q: "5-RTT 测试数据有效吗？"**

A: 有效。密钥交换流程完整，只是加密部分被 mock。时延数据准确，报文分析有效。

