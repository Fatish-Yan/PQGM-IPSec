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
│   │   ├── swanctl.conf        # 连接配置 (PSK模式)
│   │   └── strongswan.conf     # 日志配置
│   └── certs/
│       ├── x509/               # 证书 (挂载到 /usr/local/etc/swanctl/x509)
│       │   ├── encCert.pem     # 本端 SM2 加密证书
│       │   ├── signCert.pem    # 本端 SM2 签名证书
│       │   └── responder-signCert.pem  # 对端签名证书
│       ├── private/            # 私钥 (挂载到 /usr/local/etc/swanctl/private)
│       │   ├── signKey.pem     # 本端 SM2 签名私钥
│       │   └── sm2_enc_key.pem # 本端 SM2 加密私钥
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
        │   └── sm2_enc_key.pem
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

## 证书说明

### SM2 双证书机制

- **signCert.pem**: SM2 签名证书 (用于身份认证)
- **encCert.pem**: SM2 加密证书 (用于 SM2-KEM 密钥交换)
- **signKey.pem**: SM2 签名私钥
- **sm2_enc_key.pem**: SM2 加密私钥 (代码硬编码此文件名!)

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

**症状**: `cannot open /usr/local/etc/swanctl/private/sm2_enc_key.pem`

**原因**: 私钥文件名必须是 `sm2_enc_key.pem` (代码硬编码)

**解决**: 确保私钥目录中有 `sm2_enc_key.pem`

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
