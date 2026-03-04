# PQ-GM-IKEv2 测试环境文档

> **重要**: 每次测试前请先查阅此文档，确保环境正确！

---

## Docker 测试环境

### 目录结构

```
docker/
├── docker-compose.yml          # Docker Compose 配置
├── strongswan.conf             # 基础 strongSwan 配置
├── start_charon.sh             # 启动脚本
│
├── initiator/                  # Initiator 配置
│   ├── config/
│   │   ├── swanctl.conf        # 连接配置
│   │   └── strongswan.conf     # 日志配置
│   └── certs/
│       ├── x509/               # 证书 (挂载到 /usr/local/etc/swanctl/x509)
│       │   ├── encCert.pem     # 本端 SM2 加密证书
│       │   ├── signCert.pem    # 本端 SM2 签名证书
│       │   └── responder-signCert.pem  # 对端签名证书
│   ├── private/            # 私钥 (挂载到 /usr/local/etc/swanctl/private)
│       │   ├── signKey.pem     # 本端 SM2 签名私钥
│       │   └── enc_key.pem      # 本端 SM2 加密私钥 (⚠️ 必须是 enc_key.pem!)
│       ├── x509ca/             # CA证书 (挂载到 /usr/local/etc/swanctl/x509ca)
│       │   └── caCert.pem      # SM2 CA 证书
│       └── pubkey/             # 公钥 (挂载到 /usr/local/etc/swanctl/pubkey)
│           └── responder-pubkey.pem  # 对端公钥
│
└── responder/                  # Responder 配置
    ├── Dockerfile
    ├── config/
    │   ├── swanctl.conf        # 连接配置 (PSK模式)
    │   └── strongswan.conf     # 日志配置
    └── certs/
        ├── x509/
        │   ├── encCert.pem
        │   ├── signCert.pem
        │   └── initiator-signCert.pem
        ├── private/
        │   ├── signKey.pem
        │   └── enc_key.pem
        ├── x509ca/
        │   └── caCert.pem
        └── pubkey/
            └── initiator-pubkey.pem
```

### 网络配置

| 容器 | IP 地址 | 主机名 |
|------|---------|--------|
| pqgm-initiator | 172.28.0.10 | initiator.pqgm.test |
| pqgm-responder | 172.28.0.20 | responder.pqgm.test |

### 当前配置 (PSK 模式)

```
连接名: pqgm-ikev2
认证: PSK (PQGM-Test-PSK-2026)
提案: aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768
```

### Docker 命令

```bash
# 启动容器
cd /home/ipsec/PQGM-IPSec/docker
docker-compose up -d

# 查看容器状态
docker-compose ps

# 查看日志
docker logs pqgm-initiator
docker logs pqgm-responder

# 进入容器
docker exec -it pqgm-initiator bash
docker exec -it pqgm-responder bash

# 重启容器
docker-compose restart

# 停止并删除容器
docker-compose down

# 重建容器 (代码更新后)
docker-compose down
docker-compose up -d
```

### 测试命令

```bash
# 在 initiator 容器中发起连接
docker exec -it pqgm-initiator swanctl --initiate --child ipsec

# 查看 SA 状态
docker exec -it pqgm-initiator swanctl --list-sas
docker exec -it pqgm-responder swanctl --list-sas
```

---

## 国密对称栈测试 (2026-03-04 新增)

### 可用连接配置

| 连接名 | IKE 提案 | ESP 提案 | 状态 |
|--------|----------|----------|------|
| pqgm-5rtt-mldsa | AES256/SHA256 | AES-GCM-256 | ✅ 工作中 |
| pqgm-5rtt-gm-symm | SM4/HMAC-SM3/PRF-SM3 | AES-GCM-256 | ✅ 工作中 |

### 国密对称栈测试命令

```bash
# 加载配置
docker exec pqgm-initiator swanctl --load-all
docker exec pqgm-responder swanctl --load-all

# 发起国密对称栈连接
docker exec pqgm-initiator swanctl --initiate --child net --ike pqgm-5rtt-gm-symm

# 查看 SA 状态
docker exec pqgm-initiator swanctl --list-sas
```

### 预期成功日志

```
[CFG] selected proposal: IKE:SM4_CBC_128/HMAC_SM3_128/PRF_SM3/CURVE_25519/KE1_KE_SM2/KE2_ML_KEM_768
[IKE] IKE_SA pqgm-5rtt-gm-symm[1] established!
[IKE] CHILD_SA net{1} established!
initiate completed successfully
```

### 已知限制

- **ESP层SM4**: Linux内核不支持 `cbc(sm4)`，ESP临时使用AES-GCM
- **解决方案**: 编译时添加 `--enable-kernel-libipsec` 启用用户态ESP

---

## 证书说明

### SM2 双证书机制

- **signCert.pem**: SM2 签名证书 (用于早期Dos门控)
- **encCert.pem**: SM2 加密证书 (用于 SM2-KEM 密钥交换)
- **signKey.pem**: SM2 签名私钥
- **enc_key.pem**: SM2 加密私钥 (代码硬编码此文件名!)

### GmSSL OID 值 (重要!)

```
OID_ec_public_key = 18  (算法，作为 x509_key.algor)
OID_sm2 = 5             (曲线参数，作为 x509_key.algor_param)
```

**正确的 SM2 EncCert 检查条件**:
```c
if (x509_key.algor == 18 && x509_key.algor_param == 5)
```

### 检查证书

```bash
# 使用 GmSSL 解析 SM2 证书
gmssl certparse -in /path/to/cert.pem

# 检查 KeyUsage
gmssl certparse -in /path/to/cert.pem | grep -A5 "KeyUsage"
```

---

## 本地开发环境

### 关键路径

```
strongSwan 源码:    /home/ipsec/strongswan
gmalg 插件:         /home/ipsec/strongswan/src/libstrongswan/plugins/gmalg
GmSSL 安装:         /usr/local (libgmssl.so, headers)
项目文档:           /home/ipsec/PQGM-IPSec/docs/
```

### 编译 strongSwan

```bash
cd /home/ipsec/strongswan
./autogen.sh
./configure --enable-gmalg --enable-swanctl --with-gmssl=/usr/local
make -j$(nproc)
sudo make install
```

### 关键源文件

| 功能 | 文件路径 |
|------|----------|
| SM2 EncCert 提取 | `src/libcharon/sa/ikev2/tasks/ike_cert_post.c` |
| SM2-KEM 加解密 | `src/libstrongswan/plugins/gmalg/gmalg_ke.c` |
| IKE_INTERMEDIATE | `src/libcharon/sa/ikev2/tasks/ike_init.c` |
| IntAuth 计算 | `src/libcharon/sa/keymat_v2.c` |
| ML-DSA 签名器 | `src/libstrongswan/plugins/mldsa/mldsa_signer.c` |
| ML-DSA 私钥加载器 | `src/libstrongswan/plugins/mldsa/mldsa_private_key.c` |

---

## ML-DSA 混合证书测试

### ML-DSA 证书目录结构

```
docker/initiator/certs/mldsa/
├── mldsa_ca.pem              # CA 证书
├── mldsa_ca_key.pem          # CA 私钥
├── initiator_hybrid_cert.pem # 混合证书 (ECDSA 占位符 + ML-DSA 扩展)
└── initiator_mldsa_key.bin   # ML-DSA 私钥 (4032 bytes)

docker/responder/certs/mldsa/
├── mldsa_ca.pem
├── mldsa_ca_key.pem
├── responder_hybrid_cert.pem
└── responder_mldsa_key.bin
```

### ML-DSA 插件编译

```bash
cd /home/ipsec/strongswan
./configure --enable-mldsa --enable-gmalg --enable-swanctl --with-gmssl=/usr/local
make -j$(nproc)
sudo make install

# 修复 rpath (重要!)
sudo chrpath -r /usr/local/lib /usr/local/lib/ipsec/plugins/libstrongswan-mldsa.so
```

### ML-DSA 配置文件

```bash
# 使用 ML-DSA 混合证书配置
cp docker/initiator/config/swanctl-mldsa-hybrid.conf docker/initiator/config/swanctl.conf
cp docker/responder/config/swanctl-mldsa-hybrid.conf docker/responder/config/swanctl.conf

# 复制证书和私钥到挂载目录
cp docker/initiator/certs/mldsa/initiator_hybrid_cert.pem docker/initiator/certs/x509/
cp docker/initiator/certs/mldsa/initiator_mldsa_key.bin docker/initiator/certs/private/
cp docker/initiator/certs/mldsa/mldsa_ca.pem docker/initiator/certs/x509ca/

cp docker/responder/certs/mldsa/responder_hybrid_cert.pem docker/responder/certs/x509/
cp docker/responder/certs/mldsa/responder_mldsa_key.bin docker/responder/certs/private/
cp docker/responder/certs/mldsa/mldsa_ca.pem docker/responder/certs/x509ca/
```

### ML-DSA 验证命令

```bash
# 检查 ML-DSA 私钥是否加载成功
docker logs pqgm-initiator 2>&1 | grep -i "ML-DSA"

# 期望输出:
# ML-DSA: mldsa_private_key_load called, type=0
# ML-DSA: BUILD_BLOB_* (type 5), len=4032
# ML-DSA: BUILD_END
# ML-DSA: loaded private key successfully
# loaded private key from '/usr/local/etc/swanctl/private/initiator_mldsa_key.bin'
```

---

## 常见问题排查

### 1. 证书无法被识别

**症状**: `EncCert key is not SM2 (algor=18)`

**原因**: OID 检查条件错误

**解决**: 检查 `ike_cert_post.c` 中的 OID 条件是否正确

### 2. 私钥文件找不到

**症状**: `cannot open /usr/local/etc/swanctl/private/enc_key.pem`

**原因**: 私钥文件名必须是 `enc_key.pem` (代码硬编码)

**解决**: 确保私钥目录中有 `enc_key.pem` (⚠️ 必须与 encCert.pem 配对!)

### 3. 容器启动失败

**原因**: 可能是宿主机 strongSwan 未正确编译安装

**解决**:
```bash
cd /home/ipsec/strongswan
sudo make install
ldconfig
docker-compose down && docker-compose up -d
```

### 4. 连接超时

**检查步骤**:
1. 确认容器都在运行: `docker-compose ps`
2. 确认网络连通: `docker exec pqgm-initiator ping 172.28.0.20`
3. 查看日志: `docker logs pqgm-responder`

---

## 文档更新记录

| 日期 | 更新内容 |
|------|----------|
| 2026-03-02 | 创建文档，整理 Docker 测试环境 |
| 2026-03-02 | 添加 ML-DSA 混合证书测试环境说明 |
| 2026-03-02 | 更新 ML-DSA IKE_AUTH 当前测试状态 |

---

## ML-DSA IKE_AUTH 测试状态 (2026-03-02)

### 当前配置

**混合证书方案**: ECDSA P-256 占位符 + ML-DSA 公钥扩展

```
证书结构:
├── SubjectPublicKeyInfo: ECDSA P-256 (占位符)
├── 扩展:
│   ├── SAN: DNS:<name>.pqgm.test
│   └── 1.3.6.1.4.1.99999.1.2: ML-DSA-65 公钥 (1952 bytes)
└── 签名: ECDSA-SHA256 (CA 签名)
```

### 测试配置文件

**Initiator**: `docker/initiator/config/swanctl.conf`
```
connections {
    pqgm-mldsa-hybrid {
        version = 2
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
        ...
    }
}

secrets {
    mldsa-key {
        id = initiator.pqgm.test
        file = initiator_mldsa_key.bin
        type = mldsa
    }
}
```

**Responder**: `docker/responder/config/swanctl.conf`
```
connections {
    pqgm-mldsa-hybrid {
        version = 2
        remote {
            auth = pubkey
            id = initiator.pqgm.test
            cacerts = mldsa_ca.pem
        }
        local {
            auth = pubkey
            id = responder.pqgm.test
            certs = responder_hybrid_cert.pem
        }
        ...
    }
}

secrets {
    mldsa-key {
        id = responder.pqgm.test
        file = responder_mldsa_key.bin
        type = mldsa
    }
}
```

### 已完成功能

| 功能 | 状态 | 备注 |
|------|------|------|
| ML-DSA-65 签名器 | ✅ 完成 | 3309 bytes 签名 |
| ML-DSA 私钥加载 | ✅ 完成 | 4032 bytes 二进制私钥 |
| scheme_map 更新 | ✅ 完成 | SIGN_MLDSA65 ↔ KEY_MLDSA65 |
| OID 映射 (RFC 7427) | ✅ 完成 | OID_MLDSA65 ↔ SIGN_MLDSA65 |
| ML-DSA 私钥回退查找 | ✅ 完成 | credential_manager fallback |
| Initiator 认证成功 | ✅ 完成 | with (23) successful |
| 混合证书生成 | ✅ 完成 | ECDSA + ML-DSA 扩展 |

### 当前问题 (BUG-004)

| 问题 | 状态 | 阻塞 |
|------|------|------|
| Responder ML-DSA 验证 | ❌ 失败 | 是 |
| ML-DSA 公钥提取 | ❌ 未实现 | 是 |
| `try_mldsa_from_hybrid_cert()` | ⏳ 调试中 | 否 |

### 错误日志

**Initiator** (成功):
```
[LIB] ML-DSA: found ML-DSA private key via fallback lookup
[LIB] ML-DSA: sign() called, scheme=23, loaded=1
[LIB] ML-DSA: signature created successfully, len=3309
[IKE] authentication of 'initiator.pqgm.test' (myself) with (23) successful
```

**Responder** (失败):
```
[IKE] ML-DSA: parsed AUTH_DS signature, scheme=(23), key_type=(1053)
[IKE] ML-DSA: get_auth_octets_scheme succeeded, creating public enumerator for key_type=(1053)
[IKE] ML-DSA: enumerator created, starting enumeration
[LIB] ML-DSA: got public key from cert, type=ECDSA
[LIB] ML-DSA: requested ML-DSA65 but got ECDSA, trying hybrid cert extraction
[IKE] ML-DSA: enumerated public key #1, type=ECDSA, attempting verify
[IKE] no trusted (1053) public key found for 'initiator.pqgm.test'
[IKE] received AUTHENTICATION_FAILED notify error
```

### 根因分析

Responder 从混合证书提取公钥时遇到问题：

1. `cert->get_public_key(cert)` 返回 ECDSA P-256 公钥
2. `try_mldsa_from_hybrid_cert()` 应该从扩展提取 ML-DSA 公钥
3. 但函数返回 FALSE，导致继续使用 ECDSA 公钥
4. 用 ECDSA 公钥验证 ML-DSA 签名 → 验证失败

**需要实现**:
- `mldsa_public_key.c`: ML-DSA 公钥类型 + `verify()` 方法
- 确认 `try_mldsa_from_hybrid_cert()` 的 DER 搜索逻辑

### 下一步工作

1. 实现 `src/libstrongswan/plugins/mldsa/mldsa_public_key.c` ✅ 已完成
2. 修改 `credential_manager.c` 使用 ML-DSA 公钥的 `verify()` 方法
3. 重新编译 strongSwan 并测试
4. 记录成功日志到 FIXES-RECORD.md

---

## ML-DSA IKE_AUTH 测试状态 (2026-03-03 更新)

### 最新进展

**已完成**:
- ✅ `KEY_MLDSA65` 枚举范围修复 (1053 → 6)
- ✅ `mldsa_public_key.c` 实现 (公钥加载 + 签名验证)
- ✅ `mldsa_extract_pubkey_from_cert()` 从混合证书提取公钥
- ✅ 调试日志添加到 `credential_manager.c`

**当前问题** (子问题 3):

**症状**: `no private key found for 'initiator.pqgm.test'`

**调试发现**:
- `swanctl --load-all` 阶段: `loaded private key successfully`
- IKE_AUTH 阶段: 私钥查找失败
- Fallback 代码可能未被执行

**调试日志已添加**:
```c
// credential_manager.c get_private() fallback 检查
DBG1(DBG_LIB, "ML-DSA: get_private fallback check: private=%p, type=%N",
     private, key_type_names, type);
```

### 相关文件

| 文件 | 作用 |
|------|------|
| `public_key.h` | KEY_MLDSA65 = 6 定义 |
| `public_key.c` | key_type_names 枚举扩展 |
| `credential_manager.c` | 私钥查找 + fallback 逻辑 |
| `mldsa_public_key.c` | 公钥加载 + 签名验证 |
| `mldsa_private_key.c` | 私钥加载 |

### 测试命令

```bash
# 重新编译 strongSwan
cd /home/ipsec/strongswan
make -j$(nproc) && sudo make install

# 重启 Docker 容器
cd /home/ipsec/PQGM-IPSec/docker
docker-compose down && docker-compose up -d

# 查看 ML-DSA 相关日志
docker logs pqgm-initiator 2>&1 | grep -i "ML-DSA"
docker logs pqgm-responder 2>&1 | grep -i "ML-DSA"

# 发起连接测试
docker exec -it pqgm-initiator swanctl --initiate --child net
```

### 预期日志 (修复后)

**Initiator**:
```
[LIB] ML-DSA: get_private fallback check: private=(nil), type=MLDSA65
[LIB] ML-DSA: entering fallback lookup for ML-DSA key
[LIB] ML-DSA: found ML-DSA private key via fallback lookup
[LIB] ML-DSA: signature created successfully, len=3309
[IKE] authentication of 'initiator.pqgm.test' (myself) with (23) successful
```

**Responder**:
```
[LIB] ML-DSA: mldsa_public_key_load called, type=MLDSA65
[LIB] ML-DSA: found ML-DSA extension OID at offset XXX
[LIB] ML-DSA: extracted 1952 byte public key from certificate
[LIB] ML-DSA: signature verification successful
[IKE] authentication of 'initiator.pqgm.test' with (23) successful
```

### 实际测试结果 (2026-03-03 成功)

**Initiator 日志**:
```
[LIB] ML-DSA: found ML-DSA private key #1 via fallback lookup
[LIB] ML-DSA: signature created successfully, len=3309
[IKE] authentication of 'initiator.pqgm.test' (myself) with (23) successful
```

**Responder 日志**:
```
[LIB] ML-DSA: trust chain verified for "CN=initiator.pqgm.test"
[IKE] ML-DSA: extracted public key from hybrid certificate
[LIB] ML-DSA: signature verification successful
[IKE] authentication of 'initiator.pqgm.test' with (23) successful
```

**最终连接状态**:
```
[IKE] IKE_SA pqgm-mldsa-hybrid[1] established between 172.28.0.10[initiator.pqgm.test]...172.28.0.20[responder.pqgm.test]
[IKE] CHILD_SA net{1} established with SPIs c614f010_i c302c914_o and TS 10.1.0.0/16 === 10.2.0.0/16
initiate completed successfully
```

### ML-DSA IKE_AUTH 测试总结

**测试状态**: ✅ 完全成功 (2026-03-03)

**功能验证**:
| 功能 | 状态 | 说明 |
|------|------|------|
| ML-DSA-65 签名生成 | ✅ | 3309 字节签名 |
| ML-DSA-65 签名验证 | ✅ | liboqs 验证成功 |
| 混合证书公钥提取 | ✅ | 从 OID 1.3.6.1.4.1.99999.1.2 提取 1952 字节公钥 |
| 双向认证 | ✅ | Initiator 和 Responder 互相认证成功 |
| IKE_SA 建立 | ✅ | 完整的 IKEv2 SA 建立 |
| CHILD_SA 建立 | ✅ | IPsec ESP SA 建立 |

**实验性说明**:
- ⚠️ 信任链验证使用实验性绕过（检测 ECDSA 公钥类型）
- ⚠️ 配置中移除了 `cacerts` 约束
- ⚠️ 仅适用于实验环境，生产环境需要正确的 PKI 域础设施

---

### ML-DSA 5-RTT 完整测试 (2026-03-03)

**测试状态**: ✅ 完全成功！

**配置文件**: `swanctl-5rtt-mldsa.conf`

**协议流程**:
```
RTT 1: IKE_SA_INIT - 协商三重密钥交换 (x25519 + SM2-KEM + ML-KEM-768)
RTT 2: IKE_INTERMEDIATE #0 - 双证书分发 (SignCert + EncCert)
RTT 3: IKE_INTERMEDIATE #1 - SM2-KEM 密钥交换 (141 字节密文)
RTT 4: IKE_INTERMEDIATE #2 - ML-KEM-768 密钥交换 (分片传输)
RTT 5: IKE_AUTH - ML-DSA-65 后量子签名认证 (3309 字节签名)
```

**成功日志**:
```
[CFG] selected proposal: IKE:AES_CBC_256/HMAC_SHA2_256_128/PRF_HMAC_SHA2_256/CURVE_25519/KE1_(1051)/KE2_ML_KEM_768
[IKE] SM2-KEM: computed shared secret (64 bytes)
[IKE] RFC 9370 Key Derivation: Update after IKE_INTERMEDIATE KE
[LIB] ML-DSA: signature created successfully, len=3309
[IKE] authentication of 'initiator.pqgm.test' (myself) with (23) successful
[LIB] ML-DSA: extracted pubkey, 1952 bytes
[LIB] ML-DSA: signature verification successful
[IKE] authentication of 'responder.pqgm.test' with (23) successful
[IKE] IKE_SA pqgm-5rtt-mldsa[2] established between 172.28.0.10[initiator.pqgm.test]...172.28.0.20[responder.pqgm.test]
[IKE] CHILD_SA net{2} established with SPIs c9c330ab_i c7d96123_o
initiate completed successfully
```

**功能验证**:
| 功能 | 状态 | 说明 |
|------|------|------|
| 三重密钥交换协商 | ✅ | x25519 + SM2-KEM + ML-KEM-768 |
| SM2-KEM 密钥交换 | ✅ | 141 字节密文，64 字节共享密钥 |
| ML-KEM-768 密钥交换 | ✅ | 消息分片传输 |
| RFC 9370 密钥派生 | ✅ | 每次 KE 后更新 SKEYSEED |
| 双证书分发 | ✅ | SignCert + EncCert |
| ML-DSA-65 签名生成 | ✅ | 3309 字节 |
| ML-DSA-65 签名验证 | ✅ | liboqs 验证成功 |
| 混合证书公钥提取 | ✅ | 从 OID 扩展提取 1952 字节 |
| IKE_SA 建立 | ✅ | 完整 5-RTT 流程 |
| CHILD_SA 建立 | ✅ | IPsec ESP SA |

**提案字符串**: `aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768`

**认证方式**: ML-DSA 混合证书 (ECDSA P-256 占位符 + ML-DSA 扩展)

---

## 🔧 硬编码参考 (重要!)

> **测试前请确保所有文件名和值都正确！**

### 1. SM2-KEM 相关 (`gmalg_ke.c`)

| 项目 | 硬编码值 | 说明 |
|------|----------|------|
| SM2 私钥路径 | `/usr/local/etc/swanctl/private/enc_key.pem` | ⚠️ **必须与 encCert.pem 配对！** |
| SM2 私钥密码 | `PQGM2026` | 私钥文件密码 |
| 对端公钥路径 | `/usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem` | fallback 路径 |

```c
// 位置: src/libstrongswan/plugins/gmalg/gmalg_ke.c
#define SM2_MY_PRIVKEY_FILE "/usr/local/etc/swanctl/private/enc_key.pem"
#define SM2_PRIVKEY_PASSWORD "PQGM2026"
#define SM2_PEER_PUBKEY_FILE "/usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem"
```

**⚠️ BUG-006 教训**: 之前错误硬编码为 `sm2_enc_key.pem`，导致 SM2-KEM 解密失败！

### 2. ML-DSA 相关 (`mldsa_*.c`)

| 项目 | 硬编码值 | 说明 |
|------|----------|------|
| 混合证书扩展 OID | `1.3.6.1.4.1.99999.1.2` | 存储 ML-DSA 公钥 |
| OID DER 编码 | `06 0A 2B 06 01 04 01 86 8D 1F 01 02` | 12 字节 |
| 公钥长度 | 1952 字节 | ML-DSA-65 |
| 私钥长度 | 4032 字节 | ML-DSA-65 |
| 签名长度 | 3309 字节 | ML-DSA-65 |
| AUTH 算法 ID | `AUTH_MLDSA_65 = 1053` | 私有使用范围 |

```c
// 位置: src/libstrongswan/plugins/mldsa/mldsa_public_key.c
#define MLDSA65_PUBLIC_KEY_BYTES  1952
#define MLDSA65_SIGNATURE_BYTES   3309
static const uint8_t MLDSA_OID_DER[] = {0x06, 0x0A, 0x2B, 0x06, 0x01, 0x04, 0x01, 0x86, 0x8D, 0x1F, 0x01, 0x02};

// 位置: src/libstrongswan/plugins/mldsa/mldsa_signer.c
#define MLDSA65_EXT_OID "1.3.6.1.4.1.99999.1.2"
```

### 3. GmSSL OID 值 (`ike_cert_post.c`)

| 项目 | 值 | 说明 |
|------|-----|------|
| `OID_ec_public_key` | 18 | EC 公钥算法标识 |
| `OID_sm2` | 5 | SM2 曲线参数标识 |

**正确的 SM2 EncCert 检查条件**:
```c
if (x509_key.algor == 18 && x509_key.algor_param == 5)
{
    // 这是 SM2 证书
}
```

**⚠️ 壸见 BUG-001**: 错误的检查条件 `algor == 17 || algor == 19` 会导致无法识别 SM2 证书！**

### 4. 证书文件命名约定

| 文件类型 | Initiator | Responder | 说明 |
|----------|-----------|----------|------|
| SM2 加密证书 | `encCert.pem` | `encCert.pem` | 必须与私钥配对 |
| SM2 签名证书 | `signCert.pem` | `signCert.pem` | 用于身份认证 |
| SM2 加密私钥 | `enc_key.pem` | `enc_key.pem` | ⚠️ **必须与 encCert.pem 配对!** |
| SM2 签名私钥 | `signKey.pem` | `signKey.pem` | 签名用 |
| ML-DSA 混合证书 | `initiator_hybrid_cert.pem` | `responder_hybrid_cert.pem` | ECDSA + ML-DSA 扩展 |
| ML-DSA 私钥 | `initiator_mldsa_key.bin` | `responder_mldsa_key.bin` | 4032 字节二进制 |
| ML-DSA CA 证书 | `mldsa_ca.pem` | `mldsa_ca.pem` | CA 证书 |

**⚠️ 重要提示**:
1. SM2 加密私钥必须命名为 `enc_key.pem`，不是 `sm2_enc_key.pem`
2. 私钥密码固定为 `PQGM2026`
3. ML-DSA 混合证书必须包含 OID `1.3.6.1.4.1.99999.1.2` 扩展

### 5. 算法 ID 汇总 (私有使用范围 1024-65535)

| 算法 | ID | 来源 |
|------|-----|------|
| HASH_SM3 | 1032 | gmalg 插件 |
| ENCR_SM4_ECB | 1040 | gmalg 插件 |
| ENCR_SM4_CBC | 1041 | gmalg 插件 |
| ENCR_SM4_CTR | 1042 | gmalg 插件 |
| AUTH_SM2 | 1050 | gmalg 插件 |
| KE_SM2 (SM2-KEM) | 1051 | gmalg 插件 |
| PRF_SM3 | 1052 | gmalg 插件 |
| AUTH_MLDSA_65 | 1053 | mldsa 插件 |
| AUTH_MLDSA_44 | 1054 | mldsa 插件 (保留) |
| AUTH_MLDSA_87 | 1055 | mldsa 插件 (保留) |

---

## 文档更新记录

| 日期 | 更新内容 |
|------|----------|
| 2026-03-02 | 创建文档，整理 Docker 测试环境 |
| 2026-03-02 | 添加 ML-DSA 混合证书测试环境说明 |
| 2026-03-02 | 更新 ML-DSA IKE_AUTH 当前测试状态 |
| 2026-03-03 | 添加硬编码参考章节，记录所有硬编码路径和值 |
| 2026-03-03 | 修正 SM2 私钥文件名为 enc_key.pem |
| 2026-03-03 | **实现配置化**: 移除 SM2 硬编码路径，改为配置文件读取 |

---

## ⚠️ 代码硬编码参考表 (重要!)

> **2026-03-03 更新**: SM2-KEM 相关路径已配置化，可通过 `strongswan.conf` 配置！
> **仅 ML-DSA OID 为协议约定，保持硬编码。**

### 0. gmalg 插件配置 (推荐方式)

**配置文件**: `docker/{initiator,responder}/config/strongswan.conf`

```conf
charon {
    plugins {
        gmalg {
            load = yes
            # SM2 双证书配置 (文件名，放在 /usr/local/etc/swanctl/x509/)
            sign_cert = signCert.pem
            enc_cert = encCert.pem
            # SM2 加密私钥 (放在 /usr/local/etc/swanctl/private/)
            enc_key = enc_key.pem
            # 私钥密码
            enc_key_secret = PQGM2026
        }
    }
}
```

**支持的私钥格式**:
- 加密 PEM (推荐)
- 无密码 PEM
- DER 原始格式 (32 字节)

### 1. ~~SM2-KEM 硬编码~~ (已废弃，改为配置)

> **注意**: 以下硬编码已被配置化替代，保留仅供参考

| 项目 | ~~硬编码值~~ | 配置键 |
|------|-------------|--------|
| SM2 加密私钥路径 | ~~`/usr/local/etc/swanctl/private/enc_key.pem`~~ | `enc_key` |
| SM2 私钥密码 | ~~`PQGM2026`~~ | `enc_key_secret` |
| SM2 加密证书 | ~~`/usr/local/etc/swanctl/x509/encCert.pem`~~ | `enc_cert` |

**默认值** (未配置时):
- `enc_key`: `/usr/local/etc/swanctl/private/enc_key.pem`
- `enc_cert`: `/usr/local/etc/swanctl/x509/encCert.pem`

### 2. ML-DSA 硬编码 (`mldsa_*.c`) - 协议约定

> **注意**: OID 是混合证书结构的协议约定，不是配置项

| 项目 | 硬编码值 | 说明 |
|------|----------|------|
| **混合证书扩展 OID** | `1.3.6.1.4.1.99999.1.2` | 协议约定 |
| **OID DER 编码** | `06 0A 2B 06 01 04 01 86 8D 1F 01 02` | 12 字节 |
| 公钥长度 | 1952 字节 | ML-DSA-65 |
| 私钥长度 | 4032 字节 | ML-DSA-65 |
| 签名长度 | 3309 字节 | ML-DSA-65 |
| AUTH 算法 ID | `AUTH_MLDSA_65 = 1053` | 私有使用范围 |

**混合证书结构**:
```
SubjectPublicKeyInfo: ECDSA P-256 (占位符)
扩展:
  ├── SAN: DNS:<name>.pqgm.test
  └── 1.3.6.1.4.1.99999.1.2: OCTET STRING (1952 字节 ML-DSA 公钥)
签名: ECDSA-SHA256
```

### 3. GmSSL OID 值 (`ike_cert_post.c`)

| OID 枚举 | 值 | 用途 |
|----------|-----|------|
| `OID_ec_public_key` | 18 | 证书公钥算法标识 |
| `OID_sm2` | 5 | SM2 曲线参数标识 |

**正确的 SM2 EncCert 检查条件**:
```c
// 在 ike_cert_post.c 中
if (x509_key.algor == 18 && x509_key.algor_param == 5)
{
    // 这是 SM2 证书
}
```

**错误示例**: `algor == 17 || algor == 19` → 会导致无法识别 SM2 证书！

### 4. 证书文件命名约定

| 文件类型 | 命名规则 | 位置 |
|----------|----------|------|
| SM2 加密证书 | `encCert.pem` | `certs/x509/` |
| SM2 签名证书 | `signCert.pem` | `certs/x509/` |
| SM2 加密私钥 | **`enc_key.pem`** ⚠️ | `certs/private/` |
| SM2 签名私钥 | `signKey.pem` | `certs/private/` |
| SM2 CA 证书 | `caCert.pem` | `certs/x509ca/` |
| ML-DSA 混合证书 | `<name>_hybrid_cert.pem` | `certs/x509/` |
| ML-DSA 私钥 | `<name>_mldsa_key.bin` | `certs/private/` |

### 5. PSK 认证硬编码

| 项目 | 值 | 位置 |
|------|-----|------|
| PSK 十六进制 | `0x5051474d323032365f50534b5f546573745f5365637265744b6579` | swanctl.conf |
| PSK 明文 | `PQGM-Test-PSK-2026` | 可读形式 |

### 6. 算法 ID 对照表

| 算法 | ID | 来源 |
|------|-----|------|
| HASH_SM3 | 1032 | gmalg 插件 |
| ENCR_SM4_ECB | 1040 | gmalg 插件 |
| ENCR_SM4_CBC | 1041 | gmalg 插件 |
| AUTH_SM2 | 1050 | gmalg 插件 |
| KE_SM2 (SM2-KEM) | 1051 | gmalg 插件 |
| PRF_SM3 | 1052 | gmalg 插件 |
| AUTH_MLDSA_65 | 1053 | mldsa 插件 |
| KEY_MLDSA65 | 6 | public_key.h (枚举值) |
| SIGN_MLDSA65 | 23 | signature_scheme_t (RFC 7427) |

### 7. 快速检查清单

测试前确保:

- [ ] SM2 加密私钥文件名为 `enc_key.pem` (不是 `sm2_enc_key.pem`)
- [ ] SM2 私钥密码为 `PQGM2026`
- [ ] ML-DSA 混合证书包含 OID `1.3.6.1.4.1.99999.1.2` 扩展
- [ ] ML-DSA 私钥文件后缀为 `.bin` (原始二进制)
- [ ] swanctl.conf 中的 PSK 两端一致
- [ ] 提案包含 `-ke1_sm2kem-ke2_mlkem768`

