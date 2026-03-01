# 双虚拟机测试指南

## 概述

本指南说明如何通过克隆当前虚拟机来搭建双虚拟机测试环境，进行真实的网络 5-RTT PQ-GM-IKEv2 测试。

## 架构

```
┌─────────────────────────┐         ┌─────────────────────────┐
│     VM1 (Initiator)     │         │    VM2 (Responder)      │
│                         │         │                         │
│  192.168.172.132        │◄───────►│  192.168.172.xxx        │
│  (发起端)               │  网络   │  (响应端)               │
│                         │         │                         │
│  - strongSwan 6.0.4     │         │  - strongSwan 6.0.4     │
│  - GmSSL 3.1.3          │         │  - GmSSL 3.1.3          │
│  - SM2-KEM 插件         │         │  - SM2-KEM 插件         │
└─────────────────────────┘         └─────────────────────────┘
```

---

## 第一步：克隆虚拟机

### 1.1 在 VMware 中克隆

1. **关闭当前虚拟机**
   ```bash
   sudo poweroff
   ```

2. **在 VMware 中右键点击虚拟机** → **管理** → **克隆**

3. **克隆类型**：选择 **完整克隆**

4. **虚拟机名称**：`PQGM-Responder`（或你喜欢的名称）

5. **存储位置**：选择与原虚拟机不同的目录

6. **等待克隆完成**（约 5-10 分钟）

---

## 第二步：配置响应端虚拟机

### 2.1 启动克隆的虚拟机

1. 启动新克隆的虚拟机
2. 登录（用户名和密码与原虚拟机相同）

### 2.2 修改主机名

```bash
# 修改主机名
sudo hostnamectl set-hostname responder

# 编辑 hosts 文件
sudo nano /etc/hosts
# 将 127.0.1.1 后面的主机名改为 responder
```

### 2.3 修改 IP 地址（如果使用静态 IP）

**方法 A：使用 Netplan（Ubuntu 默认）**

```bash
sudo nano /etc/netplan/00-installer-config.yaml
```

修改为：
```yaml
network:
  ethernets:
    ens33:
      dhcp4: no
      addresses:
        - 192.168.172.133/24  # 修改为不同的 IP
      gateway4: 192.168.172.2
      nameservers:
        addresses:
          - 8.8.8.8
          - 114.114.114.114
  version: 2
```

应用配置：
```bash
sudo netplan apply
```

**方法 B：使用 DHCP（推荐，更简单）**

如果网络环境支持 DHCP，可以保持 DHCP 配置，只需确保两台虚拟机的 IP 不同即可。

### 2.4 重启网络

```bash
sudo systemctl restart networking
# 或者
sudo reboot
```

### 2.5 验证网络

```bash
# 查看本机 IP
ip addr show ens33

# 测试到发起端的连通性（假设发起端是 192.168.172.132）
ping 192.168.172.132
```

---

## 第三步：配置 strongSwan

### 3.1 在响应端配置 swanctl.conf

```bash
# 编辑配置文件
sudo nano /usr/local/etc/swanctl/swanctl.conf
```

替换为以下内容：

```conf
connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.172.133  # 响应端的 IP
        remote_addrs = 192.168.172.132  # 发起端的 IP
        proposals = aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768

        local {
            auth = psk
            id = responder.pqgm.test
        }

        remote {
            auth = psk
            id = initiator.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.2.0.0/16
                remote_ts = 10.1.0.0/16
                esp_proposals = aes256gcm16-sha256
                start_action = none
            }
        }
    }
}

secrets {
    ike-psk {
        id = responder.pqgm.test
        secret = "PQGM-Test-PSK-2026"
    }
    ike-psk-i {
        id = initiator.pqgm.test
        secret = "PQGM-Test-PSK-2026"
    }
}
```

### 3.2 在发起端配置 swanctl.conf

回到原始虚拟机（发起端），修改配置：

```bash
sudo nano /usr/local/etc/swanctl/swanctl.conf
```

替换为：

```conf
connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.172.132  # 发起端的 IP
        remote_addrs = 192.168.172.133  # 响应端的 IP
        proposals = aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768
        send_certreq = yes

        local {
            auth = psk
            id = initiator.pqgm.test
        }

        remote {
            auth = psk
            id = responder.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.1.0.0/16
                remote_ts = 10.2.0.0/16
                esp_proposals = aes256gcm16-sha256
                start_action = none
            }
        }
    }
}

secrets {
    ike-psk {
        id = initiator.pqgm.test
        secret = "PQGM-Test-PSK-2026"
    }
    ike-psk-r {
        id = responder.pqgm.test
        secret = "PQGM-Test-PSK-2026"
    }
}
```

---

## 第四步：配置 SM2 证书密钥

### 4.1 交换 SM2 公钥

**在响应端：**
```bash
# 复制响应端的 SM2 公钥到发起端可以访问的位置
# 方法 1: 使用 scp（需要先设置 SSH）
scp /usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem user@192.168.172.132:/tmp/

# 方法 2: 手动复制文件内容
cat /usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem
```

**在发起端：**
```bash
# 保存响应端的 SM2 公钥
sudo nano /usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem
# 粘贴响应端的公钥内容
```

### 4.2 确保密钥文件存在

确保以下文件在两台虚拟机上都存在：

**发起端：**
- `/usr/local/etc/swanctl/private/sm2_enc_key.pem` - 发起端的 SM2 私钥
- `/usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem` - 响应端的 SM2 公钥

**响应端：**
- `/usr/local/etc/swanctl/private/sm2_enc_key.pem` - 响应端的 SM2 私钥
- `/usr/local/etc/swanctl/x509/peer_sm2_pubkey.pem` - 发起端的 SM2 公钥

---

## 第五步：启动服务

### 5.1 在两台虚拟机上启动 strongSwan

```bash
# 启动 charon
sudo /usr/local/libexec/ipsec/charon &

# 或者使用 systemctl（如果配置了服务）
sudo systemctl start strongswan
```

### 5.2 加载配置

**在响应端：**
```bash
sudo swanctl --load-all
```

**在发起端：**
```bash
sudo swanctl --load-all
```

---

## 第六步：运行测试

### 6.1 从发起端发起连接

```bash
# 在发起端执行
sudo swanctl --initiate --child ipsec
```

### 6.2 查看连接状态

```bash
# 查看 IKE_SA
sudo swanctl --list-sas

# 查看 CHILD_SA
sudo swanctl --list-sas
```

### 6.3 抓包（可选）

**在发起端：**
```bash
sudo tcpdump -i ens33 -w /tmp/5rtt_dual_vm.pcap "udp port 500 or udp port 4500"
```

**在另一个终端运行测试：**
```bash
sudo swanctl --initiate --child ipsec
```

---

## 第七步：验证测试

### 7.1 检查日志

```bash
# 查看最近日志
journalctl -u strongswan -f

# 或直接查看 charon 输出
```

### 7.2 验证 SM2-KEM 共享密钥

```bash
# 发起端
sudo grep "get_shared_secret" /var/log/syslog | tail -5

# 响应端
sudo grep "get_shared_secret" /var/log/syslog | tail -5
```

### 7.3 测试数据通信

```bash
# 在发起端 ping 响应端的内网地址
ping 10.2.0.1
```

---

## 快速命令参考

### 响应端

```bash
# 启动服务
sudo /usr/local/libexec/ipsec/charon &
sleep 2
sudo swanctl --load-all

# 查看状态
sudo swanctl --list-sas

# 查看日志
sudo tail -f /var/log/syslog | grep IKE
```

### 发起端

```bash
# 启动服务
sudo /usr/local/libexec/ipsec/charon &
sleep 2
sudo swanctl --load-all

# 发起连接
sudo swanctl --initiate --child ipsec

# 查看状态
sudo swanctl --list-sas

# 终止连接
sudo swanctl --terminate --ike pqgm-ikev2
```

---

## 故障排除

### 问题 1: 连接超时

**检查：**
1. 两台虚拟机能否互相 ping 通
2. 防火墙是否开放 UDP 500 和 4500 端口
3. strongSwan 是否在运行

**解决：**
```bash
# 开放防火墙端口
sudo ufw allow 500/udp
sudo ufw allow 4500/udp

# 或者临时关闭防火墙
sudo ufw disable
```

### 问题 2: AUTH_FAILED

**检查：**
1. PSK 是否一致
2. ID 是否匹配
3. SM2 公钥是否正确交换

### 问题 3: 证书解析失败

这是正常的，SM2 证书使用国密算法签名，strongSwan 的 X.509 解析器无法识别。代码会自动 fallback 到文件加载。

---

## 预期结果

成功后，你应该看到类似以下的输出：

```
[IKE] IKE_SA pqgm-ikev2[1] established between 192.168.172.132[initiator.pqgm.test]...192.168.172.133[responder.pqgm.test]
[IKE] CHILD_SA ipsec{1} established with SPIs xxxxxxxx_i xxxxxxxx_o and TS 10.1.0.0/16 === 10.2.0.0/16
initiate completed successfully
```

---

## 性能对比

| 测试环境 | 握手时间 | 备注 |
|---------|---------|------|
| Docker (本地) | ~115 ms | 容器间通信，低延迟 |
| 双虚拟机 (同主机) | ~??? ms | 真实网络栈，需要测试 |
| 双物理机 | ~??? ms | 生产环境性能 |

请记录双虚拟机测试的实际时间数据用于论文。
