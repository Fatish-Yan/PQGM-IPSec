# PQ-GM-IKEv2 Initiator VM 配置指南

> 本文档指导如何将克隆的虚拟机配置为 Initiator (发起方)
>
> **目标读者**: 在克隆后的 Initiator VM 上运行的 AI Agent

---

## 概述

### 角色

| 虚拟机 | 角色 | IP 地址 | 主机名 |
|--------|------|---------|--------|
| 原机器 | Responder (响应方) | 192.168.172.132 | responder.pqgm.test |
| 克隆机器 | Initiator (发起方) | 192.168.172.134 | initiator.pqgm.test |

### 5-RTT 协议流程

```
RTT 1: IKE_SA_INIT      - 协商三重密钥交换 (x25519 + SM2-KEM + ML-KEM-768)
RTT 2: IKE_INTERMEDIATE #0 - 双证书分发 (SignCert + EncCert)
RTT 3: IKE_INTERMEDIATE #1 - SM2-KEM 密钥交换 (141 字节密文)
RTT 4: IKE_INTERMEDIATE #2 - ML-KEM-768 密钥交换 (分片传输)
RTT 5: IKE_AUTH        - ML-DSA-65 后量子签名认证 (3309 字节签名)
```

---

## 快速配置 (推荐)

### 一键配置脚本

```bash
# 1. 赋予执行权限
chmod +x /home/ipsec/PQGM-IPSec/vm-test/scripts/setup-initiator-vm.sh

# 2. 运行配置脚本
sudo /home/ipsec/PQGM-IPSec/vm-test/scripts/setup-initiator-vm.sh
```

脚本会自动完成以下配置：
- 修改主机名
- 配置 IP 地址
- 更新 /etc/hosts
- 复制 Initiator 证书和私钥
- 更新 swanctl.conf
- 重启 strongSwan

---

## 手动配置步骤

如果需要手动配置，请按以下步骤操作：

### 步骤 1: 修改主机名

```bash
sudo hostnamectl set-hostname initiator.pqgm.test

# 验证
hostname
# 应输出: initiator.pqgm.test
```

### 步骤 2: 配置 IP 地址

```bash
# 查看当前网络连接
nmcli con show

# 修改 IP 地址 (假设连接名为 "Wired connection 1")
sudo nmcli con mod "Wired connection 1" ipv4.addresses 192.168.172.134/24
sudo nmcli con mod "Wired connection 1" ipv4.method manual
sudo nmcli con up "Wired connection 1"

# 验证
ip addr
# 应看到 192.168.172.134
```

### 步骤 3: 更新 /etc/hosts

```bash
# 添加对端映射
echo "192.168.172.132    responder.pqgm.test" | sudo tee -a /etc/hosts
echo "192.168.172.134    initiator.pqgm.test" | sudo tee -a /etc/hosts

# 验证
cat /etc/hosts
```

### 步骤 4: 复制 Initiator 证书

```bash
# ML-DSA 混合证书
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509/initiator_hybrid_cert.pem \
    /usr/local/etc/swanctl/x509/

# SM2 证书
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509/signCert.pem \
    /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509/encCert.pem \
    /usr/local/etc/swanctl/x509/

# CA 证书
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/x509ca/mldsa_ca.pem \
    /usr/local/etc/swanctl/x509ca/
```

### 步骤 5: 复制 Initiator 私钥

```bash
# ML-DSA 私钥 (4032 bytes)
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/private/initiator_mldsa_key.bin \
    /usr/local/etc/swanctl/private/
sudo chmod 600 /usr/local/etc/swanctl/private/initiator_mldsa_key.bin

# SM2 加密私钥
sudo cp /home/ipsec/PQGM-IPSec/docker/initiator/certs/private/enc_key.pem \
    /usr/local/etc/swanctl/private/
sudo chmod 600 /usr/local/etc/swanctl/private/enc_key.pem
```

### 步骤 6: 更新 swanctl.conf

```bash
# 备份原配置
sudo cp /usr/local/etc/swanctl/swanctl.conf /usr/local/etc/swanctl/swanctl.conf.bak

# 复制 Initiator 配置
sudo cp /home/ipsec/PQGM-IPSec/vm-test/initiator/swanctl.conf \
    /usr/local/etc/swanctl/swanctl.conf
```

### 步骤 7: 重启 strongSwan

```bash
# 停止可能运行的进程
sudo pkill charon

# 重新加载库
sudo ldconfig

# 启动 strongSwan
sudo systemctl restart strongswan
# 或者直接启动 charon
# sudo /usr/local/libexec/ipsec/charon --debug-ike 2
```

---

## 验证配置

### 1. 检查证书和私钥

```bash
# 检查证书目录
ls -la /usr/local/etc/swanctl/x509/
# 应包含: initiator_hybrid_cert.pem, signCert.pem, encCert.pem

# 检查私钥目录
sudo ls -la /usr/local/etc/swanctl/private/
# 应包含: initiator_mldsa_key.bin, enc_key.pem
```

### 2. 加载配置

```bash
swanctl --load-all
```

预期输出应包含：
```
SM2-KEM: preloading SM2 private key for performance
loaded certificate from '/usr/local/etc/swanctl/x509/initiator_hybrid_cert.pem'
loaded private key from '/usr/local/etc/swanctl/private/initiator_mldsa_key.bin'
loaded connection 'pqgm-5rtt-mldsa'
loaded connection 'pqgm-5rtt-gm-symm'
successfully loaded 2 connections
```

### 3. 测试网络连通性

```bash
# Ping Responder
ping 192.168.172.132

# 检查端口
nc -zv 192.168.172.132 500
nc -zv 192.168.172.132 4500
```

### 4. 发起 5-RTT 连接

```bash
# 标准算法 (AES-256 + HMAC-SHA256)
swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 或国密对称栈 (SM4 + HMAC-SM3)
swanctl --initiate --child net --ike pqgm-5rtt-gm-symm
```

### 5. 检查 SA 状态

```bash
swanctl --list-sas
```

预期输出：
```
pqgm-5rtt-mldsa: #1, ESTABLISHED, IKEv2, ...
  local:  initiator.pqgm.test
  remote: responder.pqgm.test
  ...
  net: #1, INSTALLED, TUNNEL, ...
    10.1.0.0/16 === 10.2.0.0/16
```

---

## 成功日志示例

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
[IKE] IKE_SA pqgm-5rtt-mldsa[1] established between 192.168.172.134[initiator.pqgm.test]...192.168.172.132[responder.pqgm.test]
[IKE] CHILD_SA net{1} established with SPIs ... and TS 10.1.0.0/16 === 10.2.0.0/16
initiate completed successfully
```

---

## 常见问题

### Q1: 私钥加载失败

**症状**: `building CRED_PRIVATE_KEY - ANY failed`

**解决**: 确保私钥文件权限正确 (600) 且路径正确

```bash
sudo chmod 600 /usr/local/etc/swanctl/private/*.pem
sudo chmod 600 /usr/local/etc/swanctl/private/*.bin
```

### Q2: 证书不匹配

**症状**: `no trusted public key found for 'responder.pqgm.test'`

**解决**: 确保使用了正确的 Initiator 证书 (`initiator_hybrid_cert.pem`)

### Q3: 网络不通

**症状**: `sending packet: from ... to ... (timeout)`

**解决**:
1. 检查 IP 地址是否正确配置
2. 检查防火墙是否允许 UDP 500/4500
3. 检查 /etc/hosts 是否有正确映射

### Q4: 连接超时

**症状**: 没有收到任何响应

**解决**: 确认 Responder 已经启动并加载了配置

---

## 可用的两个测试配置

| 配置名 | IKE 加密 | PRF | 认证 |
|--------|----------|-----|------|
| `pqgm-5rtt-mldsa` | AES-256-CBC + HMAC-SHA256 | SHA256 | ML-DSA-65 |
| `pqgm-5rtt-gm-symm` | SM4-CBC + HMAC-SM3 | SM3 | ML-DSA-65 |

两个配置的区别仅在于 IKE SA 的对称加密算法，密钥交换和认证方式相同。

---

## 相关文件路径

| 文件 | 路径 |
|------|------|
| swanctl.conf | `/usr/local/etc/swanctl/swanctl.conf` |
| ML-DSA 混合证书 | `/usr/local/etc/swanctl/x509/initiator_hybrid_cert.pem` |
| ML-DSA 私钥 | `/usr/local/etc/swanctl/private/initiator_mldsa_key.bin` |
| SM2 加密私钥 | `/usr/local/etc/swanctl/private/enc_key.pem` |
| gmalg 插件配置 | `/usr/local/etc/strongswan.d/charon/gmalg.conf` |
| 项目文档 | `/home/ipsec/PQGM-IPSec/docs/` |

---

## 测试完成后

测试完成后，请收集以下数据用于论文：

1. **PCAP 抓包**:
   ```bash
   sudo tcpdump -i ens33 -w /tmp/pqgm-initiator.pcap \
     host 192.168.172.132 and \( udp port 500 or udp port 4500 \)
   ```

2. **strongSwan 日志**:
   ```bash
   cp /var/log/charon.log /tmp/charon-initiator.log
   ```

3. **SA 状态**:
   ```bash
   swanctl --list-sas > /tmp/sas-initiator.txt
   ```

---

*文档版本: 2026-03-05*
*项目: PQ-GM-IKEv2*
