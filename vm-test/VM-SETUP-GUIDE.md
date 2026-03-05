# VM克隆后配置指南

本文档说明如何在克隆VM后配置双机测试环境。

## 网络拓扑

```
┌─────────────────┐         ┌─────────────────┐
│    VM1          │         │    VM2          │
│  Initiator      │◄───────►│  Responder      │
│  (克隆机器)      │   VM    │  (当前机器)      │
│ 192.168.172.134 │  Network│ 192.168.172.132 │
└─────────────────┘         └─────────────────┘
```

## 第一步：克隆VM

1. 在VMware中选择当前VM
2. 右键 → 管理 → 克隆
3. 克隆类型：完整克隆
4. 命名：PQGM-IPSec-Initiator
5. 等待克隆完成

## 第二步：配置VM2 (Responder - 当前机器)

当前机器已配置为Responder，保持不变。

**验证配置**:
```bash
# 检查主机名
hostnamectl

# 检查IP
ip addr show ens33 | grep inet

# 应显示: 192.168.172.132
```

**应用Responder配置**:
```bash
# 复制配置文件
sudo cp /home/ipsec/PQGM-IPSec/vm-test/responder/swanctl.conf /usr/local/etc/swanctl/swanctl.conf

# 复制证书和私钥
sudo cp /home/ipsec/PQGM-IPSec/docker/responder/certs/x509/* /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/docker/responder/certs/private/* /usr/local/etc/swanctl/private/
sudo cp /home/ipsec/PQGM-IPSec/docker/responder/certs/x509ca/* /usr/local/etc/swanctl/x509ca/
sudo cp /home/ipsec/PQGM-IPSec/docker/responder/certs/mldsa/* /usr/local/etc/swanctl/x509/

# 复制strongswan.conf
sudo cp /home/ipsec/PQGM-IPSec/vm-test/strongswan.conf /usr/local/etc/strongswan.conf

# 设置权限
sudo chmod 600 /usr/local/etc/swanctl/private/*
```

## 第三步：配置VM1 (Initiator - 克隆机器)

### 3.1 网络配置

```bash
# 查看当前连接名称
nmcli con show

# 修改IP地址 (从.132改为.134)
sudo nmcli con mod "Wired connection 1" ipv4.addresses 192.168.172.134/24
sudo nmcli con mod "Wired connection 1" ipv4.method manual
sudo nmcli con up "Wired connection 1"

# 验证
ip addr show ens33 | grep inet
# 应显示: 192.168.172.134
```

### 3.2 主机名配置

```bash
# 修改主机名
sudo hostnamectl set-hostname initiator.pqgm.test

# 验证
hostnamectl
```

### 3.3 /etc/hosts配置

```bash
# 添加对端映射
echo "192.168.172.132  responder.pqgm.test" | sudo tee -a /etc/hosts
echo "192.168.172.134  initiator.pqgm.test" | sudo tee -a /etc/hosts
```

### 3.4 strongSwan配置

```bash
# 复制配置文件
sudo cp /home/ipsec/PQGM-IPSec/vm-test/initiator/swanctl.conf /usr/local/etc/swanctl/swanctl.conf

# 复制证书和私钥
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509/* /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/private/* /usr/local/etc/swanctl/private/
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509ca/* /usr/local/etc/swanctl/x509ca/
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/mldsa/* /usr/local/etc/swanctl/x509/

# 复制strongswan.conf
sudo cp /home/ipsec/PQGM-IPSec/vm-test/strongswan.conf /usr/local/etc/strongswan.conf

# 设置权限
sudo chmod 600 /usr/local/etc/swanctl/private/*
```

## 第四步：防火墙配置 (两台VM都执行)

```bash
# 使用UFW
sudo ufw allow 500/udp
sudo ufw allow 4500/udp
sudo ufw allow esp
sudo ufw reload

# 或者使用iptables
sudo iptables -A INPUT -p udp --dport 500 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 4500 -j ACCEPT
sudo iptables -A INPUT -p esp -j ACCEPT

# 保存规则
sudo netfilter-persistent save
```

## 第五步：验证网络连通性

**在VM1 (Initiator) 上**:
```bash
ping 192.168.172.132
# 应该能ping通
```

**在VM2 (Responder) 上**:
```bash
ping 192.168.172.134
# 应该能ping通
```

## 第六步：启动strongSwan

**先在VM2 (Responder) 上启动**:
```bash
# 清空日志
sudo truncate -s 0 /var/log/charon.log

# 启动服务
sudo systemctl restart strongswan

# 加载配置
swanctl --load-all

# 检查证书
swanctl --list-certs
```

**然后在VM1 (Initiator) 上启动**:
```bash
# 清空日志
sudo truncate -s 0 /var/log/charon.log

# 启动服务
sudo systemctl restart strongswan

# 加载配置
swanctl --load-all

# 检查证书
swanctl --list-certs
```

## 第七步：测试连接

**在VM1 (Initiator) 上执行**:
```bash
# 发起连接
swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 检查SA
swanctl --list-sas
```

## 快速配置脚本

将以下脚本保存到 `/tmp/setup_vm.sh` 并执行：

### Responder (VM2) 配置脚本
```bash
#!/bin/bash
# setup_responder.sh

set -e

echo "配置 Responder..."

# 复制配置
sudo cp /home/ipsec/PQGM-IPSec/vm-test/responder/swanctl.conf /usr/local/etc/swanctl/swanctl.conf
sudo cp /home/ipsec/PQGM-IPSec/vm-test/strongswan.conf /usr/local/etc/strongswan.conf

# 复制证书
sudo cp /home/ipsec/PQGM-IPSec/docker/responder/certs/x509/* /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/docker/responder/certs/private/* /usr/local/etc/swanctl/private/
sudo cp /home/ipsec/PQGM-IPSec/docker/responder/certs/x509ca/* /usr/local/etc/swanctl/x509ca/

# 设置权限
sudo chmod 600 /usr/local/etc/swanctl/private/*

# 配置防火墙
sudo ufw allow 500/udp
sudo ufw allow 4500/udp
sudo ufw allow esp

# 添加hosts
echo "192.168.172.134  initiator.pqgm.test" | sudo tee -a /etc/hosts

echo "Responder配置完成!"
```

### Initiator (VM1) 配置脚本
```bash
#!/bin/bash
# setup_initiator.sh

set -e

echo "配置 Initiator..."

# 修改IP
sudo nmcli con mod "Wired connection 1" ipv4.addresses 192.168.172.134/24
sudo nmcli con mod "Wired connection 1" ipv4.method manual
sudo nmcli con up "Wired connection 1"

# 修改主机名
sudo hostnamectl set-hostname initiator.pqgm.test

# 复制配置
sudo cp /home/ipsec/PQGM-IPSec/vm-test/initiator/swanctl.conf /usr/local/etc/swanctl/swanctl.conf
sudo cp /home/ipsec/PQGM-IPSec/vm-test/strongswan.conf /usr/local/etc/strongswan.conf

# 复制证书
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509/* /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/private/* /usr/local/etc/swanctl/private/
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509ca/* /usr/local/etc/swanctl/x509ca/

# 设置权限
sudo chmod 600 /usr/local/etc/swanctl/private/*

# 配置防火墙
sudo ufw allow 500/udp
sudo ufw allow 4500/udp
sudo ufw allow esp

# 添加hosts
echo "192.168.172.132  responder.pqgm.test" | sudo tee -a /etc/hosts
echo "192.168.172.134  initiator.pqgm.test" | sudo tee -a /etc/hosts

echo "Initiator配置完成!"
echo "请重启网络或重启系统以应用IP变更"
```

## 证书和私钥清单

### Initiator (VM1 - 192.168.172.134)

| 组件 | 文件 | 说明 |
|------|------|------|
| ML-DSA混合证书 | initiator_hybrid_cert.pem | CN=initiator.pqgm.test |
| ML-DSA私钥 | initiator_mldsa_key.bin | 4032 bytes |
| SM2签名证书 | sign_cert.pem | CN=initiator.pqgm-sign |
| SM2签名私钥 | sign_key.pem | 加密PEM |
| SM2加密证书 | enc_cert.pem | CN=initiator.pqgm-enc |
| SM2加密私钥 | enc_key.pem | 加密PEM (密码: PQGM2026) |
| SM2 CA | caCert.pem | CN=PQGM-SM2-CA |
| ML-DSA CA | mldsa_ca.pem | CN=PQGM-MLDSA-CA |

### Responder (VM2 - 192.168.172.132)

| 组件 | 文件 | 说明 |
|------|------|------|
| ML-DSA混合证书 | responder_hybrid_cert.pem | CN=responder.pqgm.test |
| ML-DSA私钥 | responder_mldsa_key.bin | 4032 bytes |
| SM2签名证书 | sign_cert.pem | CN=responder.pqgm-sign |
| SM2签名私钥 | sign_key.pem | 加密PEM |
| SM2加密证书 | enc_cert.pem | CN=responder.pqgm-enc |
| SM2加密私钥 | enc_key.pem | 加密PEM (密码: PQGM2026) |
| SM2 CA | caCert.pem | CN=PQGM-SM2-CA |
| ML-DSA CA | mldsa_ca.pem | CN=PQGM-MLDSA-CA |

### 重要: SM2-KEM 私钥预加载配置

> **必须配置**: 否则 SM2-KEM 性能会退化 22 倍！

在 `/usr/local/etc/strongswan.d/charon/gmalg.conf` 中添加:
```conf
gmalg {
    load = yes
    enc_key = enc_key.pem
    enc_key_secret = PQGM2026
}
```

验证预加载成功:
```
SM2-KEM: preloaded SM2 private key successfully (encrypted PEM)
```

### OpenSSL 验证说明

> **注意**: OpenSSL `verify` 命令报错是**正常的**！

- **ML-DSA 证书**: OpenSSL 不支持 ML-DSA 签名，由 strongSwan `mldsa` 插件处理
- **SM2 证书**: OpenSSL verify 不支持 SM2 签名，由 strongSwan `gmalg` 插件处理
- 证书验证在 strongSwan 内部完成

## 常见问题

### Q: 无法ping通对端
A: 检查VMware网络模式是否为NAT或桥接，确保两台VM在同一网络

### Q: swanctl --initiate 报错 "no config found"
A: 检查swanctl.conf中的remote_addrs是否正确，执行 `swanctl --load-all`

### Q: 证书验证失败
A: 确保CA证书已正确复制到x509ca目录，检查证书ID与配置中的id匹配
