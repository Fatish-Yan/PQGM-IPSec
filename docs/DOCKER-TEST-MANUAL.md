# PQ-GM-IKEv2 Docker 测试操作手册

> 本手册提供从零开始部署和测试 PQ-GM-IKEv2 5-RTT 协议的完整流程
>
> 适用于：在新环境中快速复现测试、演示部署
>
> 最后更新：2026-03-05

---

## 目录

1. [系统要求](#1-系统要求)
2. [环境准备](#2-环境准备)
3. [编译安装](#3-编译安装)
4. [证书准备](#4-证书准备)
5. [配置文件](#5-配置文件)
6. [Docker 测试](#6-docker-测试)
7. [测试验证](#7-测试验证)
8. [常见问题](#8-常见问题)

---

## 1. 系统要求

### 1.1 操作系统

- Ubuntu 22.04 LTS (推荐)
- 或其他支持 Docker 的 Linux 发行版

### 1.2 硬件要求

- CPU: 2 核以上
- 内存: 4GB 以上
- 磁盘: 20GB 以上

### 1.3 软件依赖

```bash
# 基础工具
sudo apt update
sudo apt install -y build-essential git cmake libtool autoconf \
    pkg-config libssl-dev wget curl

# Docker
sudo apt install -y docker.io docker-compose
sudo usermod -aG docker $USER
# 注销后重新登录以生效
```

---

## 2. 环境准备

### 2.1 克隆项目

```bash
git clone https://github.com/Fatish-Yan/PQGM-IPSec.git
cd PQGM-IPSec
```

### 2.2 安装 GmSSL (国密算法库)

```bash
# 克隆 GmSSL 3.1.1
git clone https://github.com/guanzhi/GmSSL.git
cd GmSSL
git checkout v3.1.1

# 编译安装
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install

# 更新库缓存
sudo ldconfig

# 验证安装
gmssl version
# 应输出: GmSSL 3.1.1

cd ../..
```

### 2.3 安装 liboqs (后量子算法库)

```bash
# 安装依赖
sudo apt install -y liboqs-dev

# 或从源码编译 (如需最新版本)
# git clone https://github.com/open-quantum-safe/liboqs.git
# cd liboqs
# mkdir build && cd build
# cmake -GNinja -DCMAKE_INSTALL_PREFIX=/usr/local ..
# ninja
# sudo ninja install
# cd ../..
```

---

## 3. 编译安装

### 3.1 获取 strongSwan 源码

```bash
# 克隆 strongSwan (或使用项目中的版本)
git clone https://github.com/strongswan/strongswan.git
cd strongswan
git checkout 6.0.4  # 或项目测试过的版本
```

### 3.2 应用 PQ-GM-IKEv2 补丁

将项目中的插件代码复制到 strongSwan 源码目录：

```bash
# 假设项目结构为:
# PQGM-IPSec/
# ├── strongswan-patches/
# │   ├── gmalg/          # 国密算法插件
# │   ├── mldsa/          # ML-DSA 签名插件
# │   └── ikev2-patches/  # IKEv2 协议扩展补丁
# └── GmSSL/

# 复制 gmalg 插件
cp -r /path/to/PQGM-IPSec/strongswan-patches/gmalg \
    src/libstrongswan/plugins/gmalg

# 复制 mldsa 插件
cp -r /path/to/PQGM-IPSec/strongswan-patches/mldsa \
    src/libstrongswan/plugins/mldsa

# 应用 IKEv2 协议补丁 (如有)
# patch -p1 < /path/to/PQGM-IPSec/strongswan-patches/ikev2.patch
```

### 3.3 配置和编译

```bash
# 生成配置脚本
./autogen.sh

# 配置 (启用 gmalg 和 mldsa 插件)
./configure \
    --enable-gmalg \
    --enable-mldsa \
    --enable-swanctl \
    --with-gmssl=/usr/local \
    --prefix=/usr/local \
    --libexecdir=/usr/local/libexec/ipsec

# 编译
make -j$(nproc)

# 安装
sudo make install

# 更新库缓存
sudo ldconfig
```

### 3.4 验证安装

```bash
# 检查 charon 可执行文件
ls -la /usr/local/libexec/ipsec/charon

# 检查插件
ls -la /usr/local/lib/ipsec/plugins/ | grep -E "gmalg|mldsa"

# 检查 swanctl
swanctl --version
```

---

## 4. 证书准备

### 4.1 目录结构

```
docker/
├── initiator/
│   └── certs/
│       ├── x509/           # 端实体证书
│       │   ├── initiator_hybrid_cert.pem  # ML-DSA 混合证书
│       │   ├── signCert.pem               # SM2 签名证书
│       │   └── encCert.pem                # SM2 加密证书
│       ├── private/        # 私钥
│       │   ├── initiator_mldsa_key.bin    # ML-DSA 私钥 (4032 bytes)
│       │   └── enc_key.pem                # SM2 加密私钥
│       ├── x509ca/         # CA 证书
│       │   └── mldsa_ca.pem
│       └── pubkey/         # 对端公钥
│           └── responder-pubkey.pem
│
└── responder/
    └── certs/
        ├── x509/
        │   ├── responder_hybrid_cert.pem
        │   ├── signCert.pem
        │   └── encCert.pem
        ├── private/
        │   ├── responder_mldsa_key.bin
        │   └── enc_key.pem
        ├── x509ca/
        │   └── mldsa_ca.pem
        └── pubkey/
            └── initiator-pubkey.pem
```

### 4.2 生成 ML-DSA 混合证书

```bash
cd /path/to/PQGM-IPSec/docker/certs

# 1. 生成 ML-DSA CA 私钥
openssl genpkey -algorithm ML-DSA-65 -out mldsa_ca_key.bin

# 2. 生成 ML-DSA CA 证书
# 注意: 需要支持 ML-DSA 的 OpenSSL 或使用 liboqs 工具

# 3. 生成混合证书 (ECDSA 占位符 + ML-DSA 扩展)
# 使用项目提供的脚本:
./generate_hybrid_certs.sh
```

### 4.3 生成 SM2 证书

```bash
# 使用 GmSSL 生成 SM2 证书
gmssl sm2keygen -pass PQGM2026 -out enc_key.pem

# 生成 SM2 证书签名请求
gmssl req -new -key enc_key.pem -passin PQGM2026 -out enc.csr

# 签发证书
gmssl x509 -req -in enc.csr -CA caCert.pem -CAkey caKey.pem \
    -CAcreateserial -out encCert.pem -days 365
```

### 4.4 关键文件说明

| 文件 | 格式 | 大小 | 用途 |
|------|------|------|------|
| `*_hybrid_cert.pem` | PEM (DER) | ~2.4KB | ML-DSA 混合证书 |
| `*_mldsa_key.bin` | RAW | 4032 bytes | ML-DSA 私钥 |
| `enc_key.pem` | PEM (加密) | ~436 bytes | SM2 加密私钥 |
| `signCert.pem` | PEM | ~575 bytes | SM2 签名证书 |
| `encCert.pem` | PEM | ~575 bytes | SM2 加密证书 |
| `mldsa_ca.pem` | PEM | ~497 bytes | ML-DSA CA 证书 |

**重要**:
- SM2 加密私钥密码固定为 `PQGM2026`
- ML-DSA 混合证书包含 OID `1.3.6.1.4.1.99999.1.2` 扩展

---

## 5. 配置文件

### 5.1 swanctl.conf (Initiator)

位置: `vm-test/docker/initiator/swanctl.conf`

```conf
connections {
    pqgm-5rtt-mldsa {
        version = 2
        local_addrs = 172.30.0.10
        remote_addrs = 172.30.0.20
        proposals = aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768

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
    }
    sm2-enc {
        id = initiator.pqgm.test
        file = enc_key.pem
    }
}
```

### 5.2 swanctl.conf (Responder)

位置: `vm-test/docker/responder/swanctl.conf`

```conf
connections {
    pqgm-5rtt-mldsa {
        version = 2
        local_addrs = 172.30.0.20
        remote_addrs = 172.30.0.10
        proposals = aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768

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
    }
    sm2-enc {
        id = responder.pqgm.test
        file = enc_key.pem
    }
}
```

### 5.3 gmalg.conf (插件配置)

位置: `vm-test/gmalg.conf`

```conf
gmalg {
    load = yes
    # SM2 加密私钥配置
    enc_key = enc_key.pem
    enc_key_secret = PQGM2026
    enc_cert = encCert.pem
    sign_cert = signCert.pem
}
```

### 5.4 Docker Compose

位置: `vm-test/docker-compose-test.yml`

```yaml
version: '3.8'

services:
  initiator:
    image: ubuntu:22.04
    container_name: pqgm-initiator-test
    hostname: initiator.pqgm.test
    networks:
      pqgm_net:
        ipv4_address: 172.30.0.10
    privileged: true
    cap_add:
      - NET_ADMIN
      - SYS_MODULE
    tmpfs:
      - /var/run:exec
      - /tmp
    volumes:
      - /usr/local:/usr/local:ro
      - /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509:/usr/local/etc/swanctl/x509:ro
      - /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509ca:/usr/local/etc/swanctl/x509ca:ro
      - /home/ipsec/PQGM-IPSec/docker/initiator/certs/private:/usr/local/etc/swanctl/private:ro
      - /home/ipsec/PQGM-IPSec/docker/initiator/certs/pubkey:/usr/local/etc/swanctl/pubkey:ro
      - /home/ipsec/PQGM-IPSec/vm-test/docker/initiator/swanctl.conf:/usr/local/etc/swanctl/swanctl.conf:ro
      - /home/ipsec/PQGM-IPSec/vm-test/gmalg.conf:/usr/local/etc/strongswan.d/charon/gmalg.conf:ro
      - /lib:/lib:ro
      - /lib64:/lib64:ro
      - /usr/lib:/usr/lib:ro
    command: /bin/bash -c "ldconfig && exec /usr/local/libexec/ipsec/charon --debug-ike 2"

  responder:
    image: ubuntu:22.04
    container_name: pqgm-responder-test
    hostname: responder.pqgm.test
    networks:
      pqgm_net:
        ipv4_address: 172.30.0.20
    privileged: true
    cap_add:
      - NET_ADMIN
      - SYS_MODULE
    tmpfs:
      - /var/run:exec
      - /tmp
    volumes:
      - /usr/local:/usr/local:ro
      - /home/ipsec/PQGM-IPSec/docker/responder/certs/x509:/usr/local/etc/swanctl/x509:ro
      - /home/ipsec/PQGM-IPSec/docker/responder/certs/x509ca:/usr/local/etc/swanctl/x509ca:ro
      - /home/ipsec/PQGM-IPSec/docker/responder/certs/private:/usr/local/etc/swanctl/private:ro
      - /home/ipsec/PQGM-IPSec/docker/responder/certs/pubkey:/usr/local/etc/swanctl/pubkey:ro
      - /home/ipsec/PQGM-IPSec/vm-test/docker/responder/swanctl.conf:/usr/local/etc/swanctl/swanctl.conf:ro
      - /home/ipsec/PQGM-IPSec/vm-test/gmalg.conf:/usr/local/etc/strongswan.d/charon/gmalg.conf:ro
      - /lib:/lib:ro
      - /lib64:/lib64:ro
      - /usr/lib:/usr/lib:ro
    command: /bin/bash -c "ldconfig && exec /usr/local/libexec/ipsec/charon --debug-ike 2"

networks:
  pqgm_net:
    driver: bridge
    ipam:
      config:
        - subnet: 172.30.0.0/24
```

---

## 6. Docker 测试

### 6.1 启动容器

```bash
cd /path/to/PQGM-IPSec/vm-test

# 启动容器
sudo docker-compose -f docker-compose-test.yml up -d

# 查看容器状态
sudo docker-compose -f docker-compose-test.yml ps

# 查看日志
sudo docker logs pqgm-initiator-test
sudo docker logs pqgm-responder-test
```

### 6.2 加载配置

```bash
# 在两个容器中加载配置
sudo docker exec pqgm-initiator-test swanctl --load-all
sudo docker exec pqgm-responder-test swanctl --load-all
```

预期输出：
```
SM2-KEM: preloading SM2 private key for performance
loaded certificate from '/usr/local/etc/swanctl/x509/initiator_hybrid_cert.pem'
loaded certificate from '/usr/local/etc/swanctl/x509ca/mldsa_ca.pem'
loaded private key from '/usr/local/etc/swanctl/private/initiator_mldsa_key.bin'
loaded connection 'pqgm-5rtt-mldsa'
loaded connection 'pqgm-5rtt-gm-symm'
successfully loaded 2 connections, 0 unloaded
```

### 6.3 发起连接

```bash
# 标准算法测试 (AES-256 + HMAC-SHA256)
sudo docker exec pqgm-initiator-test swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 国密对称栈测试 (SM4 + HMAC-SM3)
sudo docker exec pqgm-initiator-test swanctl --initiate --child net --ike pqgm-5rtt-gm-symm
```

### 6.4 停止容器

```bash
sudo docker-compose -f docker-compose-test.yml down
```

---

## 7. 测试验证

### 7.1 成功标志

连接成功时应看到以下关键日志：

```
[CFG] selected proposal: IKE:AES_CBC_256/HMAC_SHA2_256_128/PRF_HMAC_SHA2_256/CURVE_25519/KE1_KE_SM2/KE2_ML_KEM_768
[IKE] SM2-KEM: computed shared secret (64 bytes)
[IKE] RFC 9370 Key Derivation: Update after IKE_INTERMEDIATE KE
[LIB] ML-DSA: signature created successfully, len=3309
[IKE] authentication of 'initiator.pqgm.test' (myself) with (23) successful
[LIB] ML-DSA: signature verification successful
[IKE] authentication of 'responder.pqgm.test' with (23) successful
[LIB] ML-DSA: CA constraint bypass for hybrid certificate (ECDSA placeholder detected), peer authenticated
[IKE] IKE_SA pqgm-5rtt-mldsa[1] established between 172.30.0.10[initiator.pqgm.test]...172.30.0.20[responder.pqgm.test]
[IKE] CHILD_SA net{1} established with SPIs c8d9e98b_i cdb3f6b7_o and TS 10.1.0.0/16 === 10.2.0.0/16
initiate completed successfully
```

### 7.2 验证 SA 状态

```bash
sudo docker exec pqgm-initiator-test swanctl --list-sas
```

预期输出：
```
pqgm-5rtt-mldsa: #1, ESTABLISHED, IKEv2, ...
  local:  172.30.0.10[initiator.pqgm.test]
  remote: 172.30.0.20[responder.pqgm.test]
  ...
  net: #1, INSTALLED, TUNNEL, ...
    10.1.0.0/16 === 10.2.0.0/16
```

### 7.3 5-RTT 流程验证

| RTT | 阶段 | 验证日志 |
|-----|------|----------|
| 1 | IKE_SA_INIT | `selected proposal: ...KE1_KE_SM2/KE2_ML_KEM_768` |
| 2 | IKE_INTERMEDIATE #0 | `sending SignCert certificate` / `sending EncCert certificate` |
| 3 | IKE_INTERMEDIATE #1 | `SM2-KEM: computed shared secret (64 bytes)` |
| 4 | IKE_INTERMEDIATE #2 | `RFC 9370 Key Derivation: Update after IKE_INTERMEDIATE KE` |
| 5 | IKE_AUTH | `ML-DSA: signature verification successful` |

---

## 8. 常见问题

### 8.1 私钥加载失败

**症状**: `building CRED_PRIVATE_KEY - ANY failed`

**原因**: 私钥文件格式不正确或路径错误

**解决**:
```bash
# 检查私钥文件
ls -la /usr/local/etc/swanctl/private/

# 确保文件存在且权限正确
sudo chmod 600 /usr/local/etc/swanctl/private/*
```

### 8.2 证书不被信任

**症状**: `constraint check failed: peer not authenticated by CA`

**原因**: ML-DSA 混合证书与标准 PKI 验证不兼容

**解决**: 确保已应用 CA 约束绕过补丁 (auth_cfg.c)

### 8.3 SM2-KEM 失败

**症状**: `SM2-KEM: failed to decrypt`

**原因**: SM2 私钥与证书不匹配，或密码错误

**解决**:
```bash
# 检查私钥密码配置
cat /usr/local/etc/strongswan.d/charon/gmalg.conf

# 确保密码为 PQGM2026
```

### 8.4 网络不通

**症状**: `sending packet: ... (timeout)`

**解决**:
```bash
# 检查容器网络
sudo docker network ls
sudo docker network inspect vm-test_pqgm_net

# 检查防火墙
sudo ufw status
```

### 8.5 插件加载失败

**症状**: `plugin 'gmalg' failed to load`

**解决**:
```bash
# 检查 GmSSL 库
ldconfig -p | grep gmssl

# 检查插件文件
ls -la /usr/local/lib/ipsec/plugins/libstrongswan-gmalg.so
ls -la /usr/local/lib/ipsec/plugins/libstrongswan-mldsa.so

# 检查 rpath
sudo chrpath -l /usr/local/lib/ipsec/plugins/libstrongswan-gmalg.so
```

---

## 9. 快速复现脚本

将以下脚本保存为 `quick-test.sh`：

```bash
#!/bin/bash
set -e

PROJECT_DIR="/home/ipsec/PQGM-IPSec"
VMTEST_DIR="${PROJECT_DIR}/vm-test"

echo "=== PQ-GM-IKEv2 Docker 快速测试 ==="

# 1. 停止旧容器
echo "[1/5] 停止旧容器..."
sudo docker-compose -f "${VMTEST_DIR}/docker-compose-test.yml" down 2>/dev/null || true

# 2. 启动新容器
echo "[2/5] 启动容器..."
sudo docker-compose -f "${VMTEST_DIR}/docker-compose-test.yml" up -d

# 3. 等待启动
echo "[3/5] 等待 charon 启动..."
sleep 3

# 4. 加载配置
echo "[4/5] 加载配置..."
sudo docker exec pqgm-initiator-test swanctl --load-all
sudo docker exec pqgm-responder-test swanctl --load-all

# 5. 发起连接
echo "[5/5] 发起 5-RTT 连接..."
sudo docker exec pqgm-initiator-test swanctl --initiate --child net --ike pqgm-5rtt-mldsa

echo ""
echo "=== 测试完成 ==="
echo "查看 SA 状态: sudo docker exec pqgm-initiator-test swanctl --list-sas"
```

---

## 附录 A: 算法 ID 对照表

| 算法 | ID | 来源 |
|------|-----|------|
| HASH_SM3 | 1032 | gmalg 插件 |
| ENCR_SM4_CBC | 1041 | gmalg 插件 |
| AUTH_SM2 | 1050 | gmalg 插件 |
| KE_SM2 (SM2-KEM) | 1051 | gmalg 插件 |
| PRF_SM3 | 1052 | gmalg 插件 |
| AUTH_MLDSA_65 | 1053 | mldsa 插件 |
| KEY_MLDSA65 | 6 | public_key.h |
| SIGN_MLDSA65 | 23 | RFC 7427 |

## 附录 B: 网络拓扑

```
┌─────────────────────┐                    ┌─────────────────────┐
│  pqgm-initiator-test│                    │  pqgm-responder-test│
│  172.30.0.10        │◄──────────────────►│  172.30.0.20        │
│  initiator.pqgm.test│   Docker Bridge    │  responder.pqgm.test│
│                     │    172.30.0.0/24   │                     │
│  10.1.0.0/16 (VPN)  │                    │  10.2.0.0/16 (VPN)  │
└─────────────────────┘                    └─────────────────────┘
```

## 附录 C: 提案字符串说明

```
aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768
│      │      │       │          │
│      │      │       │          └── ADDKE2: ML-KEM-768
│      │      │       └── ADDKE1: SM2-KEM
│      │      └── KE: x25519
│      └── PRF/Integrity: HMAC-SHA256
└── Encryption: AES-256-CBC
```

---

*文档版本: 2026-03-05*
*项目: PQ-GM-IKEv2 - 抗量子与国密融合的 IKEv2/IPSec 协议*
