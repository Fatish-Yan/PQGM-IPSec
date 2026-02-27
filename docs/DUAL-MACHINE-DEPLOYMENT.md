# PQ-GM-IKEv2 双机部署测试指南

> 创建时间: 2026-02-27
> 适用场景: 克隆虚拟机进行双机测试

---

## 1. 测试环境

### 1.1 虚拟机配置

| 角色 | IP 地址 | 主机名 |
|------|---------|--------|
| Initiator | 192.168.1.10 | initiator.pqgm.test |
| Responder | 192.168.1.20 | responder.pqgm.test |

### 1.2 克隆步骤

1. 关闭当前虚拟机
2. 克隆虚拟机 (完整克隆)
3. 启动两台虚拟机
4. 分别配置网络

---

## 2. Initiator 配置

### 2.1 网络配置

```bash
# 设置静态 IP
sudo nmcli con mod ens33 ipv4.addresses 192.168.1.10/24
sudo nmcli con mod ens33 ipv4.gateway 192.168.1.1
sudo nmcli con mod ens33 ipv4.dns "8.8.8.8"
sudo nmcli con mod ens33 ipv4.method manual
sudo nmcli con up ens33

# 设置主机名
sudo hostnamectl set-hostname initiator.pqgm.test

# 添加 hosts 记录
echo "192.168.1.10 initiator.pqgm.test" | sudo tee -a /etc/hosts
echo "192.168.1.20 responder.pqgm.test" | sudo tee -a /etc/hosts
```

### 2.2 strongSwan 配置

**编辑 `/usr/local/etc/strongswan.conf`**:
```bash
sudo tee /usr/local/etc/strongswan.conf << 'EOF'
# strongswan.conf - strongSwan configuration file

charon {
    load_modular = yes
    plugins {
        include strongswan.d/charon/*.conf
    }

    # Accept private use transform IDs (for SM2-KEM = 1051)
    accept_private_algs = yes
}

include strongswan.d/*.conf
EOF
```

### 2.3 swanctl 配置

**创建 `/usr/local/etc/swanctl/swanctl.conf`**:
```bash
sudo tee /usr/local/etc/swanctl/swanctl.conf << 'EOF'
# PQ-GM-IKEv2 Initiator Configuration
# Triple Key Exchange: x25519 + ML-KEM-768 + SM2-KEM

connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.1.10
        remote_addrs = 192.168.1.20

        # 三重密钥交换
        proposals = aes256-sha256-x25519-ke1_mlkem768-ke2_sm2kem

        local {
            auth = pubkey
            certs = signCert.pem
            id = initiator-sign.pqgm.test
        }

        remote {
            auth = pubkey
            id = responder-sign.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.1.0.0/16
                remote_ts = 10.2.0.0/16

                esp_proposals = aes256gcm16-sha256

                updown = /usr/local/libexec/ipsec/_updown iptables
                start_action = none
            }
        }
    }
}

secrets {
    sign {
        file = signKey.pem
    }
    enc {
        file = encKey.pem
    }
}
EOF
```

### 2.4 部署证书

```bash
# 清理旧证书
sudo rm -f /usr/local/etc/swanctl/x509/*
sudo rm -f /usr/local/etc/swanctl/x509ca/*
sudo rm -f /usr/local/etc/swanctl/private/*

# 部署 Initiator 证书
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/ca/caCert.pem /usr/local/etc/swanctl/x509ca/
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/initiator/signCert.pem /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/initiator/encCert.pem /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/initiator/signKey.pem /usr/local/etc/swanctl/private/
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/initiator/encKey.pem /usr/local/etc/swanctl/private/

# 验证
ls -la /usr/local/etc/swanctl/x509/
ls -la /usr/local/etc/swanctl/private/
```

---

## 3. Responder 配置

### 3.1 网络配置

```bash
# 设置静态 IP
sudo nmcli con mod ens33 ipv4.addresses 192.168.1.20/24
sudo nmcli con mod ens33 ipv4.gateway 192.168.1.1
sudo nmcli con mod ens33 ipv4.dns "8.8.8.8"
sudo nmcli con mod ens33 ipv4.method manual
sudo nmcli con up ens33

# 设置主机名
sudo hostnamectl set-hostname responder.pqgm.test

# 添加 hosts 记录
echo "192.168.1.10 initiator.pqgm.test" | sudo tee -a /etc/hosts
echo "192.168.1.20 responder.pqgm.test" | sudo tee -a /etc/hosts
```

### 3.2 strongSwan 配置

**编辑 `/usr/local/etc/strongswan.conf`**:
```bash
sudo tee /usr/local/etc/strongswan.conf << 'EOF'
# strongswan.conf - strongSwan configuration file

charon {
    load_modular = yes
    plugins {
        include strongswan.d/charon/*.conf
    }

    # Accept private use transform IDs (for SM2-KEM = 1051)
    accept_private_algs = yes
}

include strongswan.d/*.conf
EOF
```

### 3.3 swanctl 配置

**创建 `/usr/local/etc/swanctl/swanctl.conf`**:
```bash
sudo tee /usr/local/etc/swanctl/swanctl.conf << 'EOF'
# PQ-GM-IKEv2 Responder Configuration
# Triple Key Exchange: x25519 + ML-KEM-768 + SM2-KEM

connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.1.20

        # 三重密钥交换
        proposals = aes256-sha256-x25519-ke1_mlkem768-ke2_sm2kem

        local {
            auth = pubkey
            certs = signCert.pem
            id = responder-sign.pqgm.test
        }

        remote {
            auth = pubkey
            id = initiator-sign.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.2.0.0/16
                remote_ts = 10.1.0.0/16

                esp_proposals = aes256gcm16-sha256

                updown = /usr/local/libexec/ipsec/_updown iptables
            }
        }
    }
}

secrets {
    sign {
        file = signKey.pem
    }
    enc {
        file = encKey.pem
    }
}
EOF
```

### 3.4 部署证书

```bash
# 清理旧证书
sudo rm -f /usr/local/etc/swanctl/x509/*
sudo rm -f /usr/local/etc/swanctl/x509ca/*
sudo rm -f /usr/local/etc/swanctl/private/*

# 部署 Responder 证书
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/ca/caCert.pem /usr/local/etc/swanctl/x509ca/
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/responder/signCert.pem /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/responder/encCert.pem /usr/local/etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/responder/signKey.pem /usr/local/etc/swanctl/private/
sudo cp /home/ipsec/PQGM-IPSec/certs-pqgm/responder/encKey.pem /usr/local/etc/swanctl/private/

# 验证
ls -la /usr/local/etc/swanctl/x509/
ls -la /usr/local/etc/swanctl/private/
```

---

## 4. 启动和测试

### 4.1 启动 strongSwan (两台机器)

```bash
# 停止可能存在的进程
sudo pkill charon 2>/dev/null || true

# 启动 charon
sudo /usr/local/libexec/ipsec/charon &

# 等待启动
sleep 3

# 加载配置
sudo swanctl --load-all
```

### 4.2 验证配置加载

```bash
# 检查插件
sudo swanctl --stats | grep "loaded plugins"

# 应该看到: loaded plugins: charon gmalg random nonce x509 ... ml ...
```

### 4.3 发起连接 (Initiator)

```bash
# 在 Initiator 上执行
sudo swanctl --initiate --child ipsec
```

### 4.4 预期输出

成功的输出应该包含：

```
[IKE] initiating IKE_SA pqgm-ikev2[1] to 192.168.1.20
[CFG] selected proposal: IKE:AES_CBC_256/.../CURVE_25519/KE1_ML_KEM_768/KE2_SM2KEM
[IKE] PQ-GM-IKEv2: will send certificates in IKE_INTERMEDIATE #0
[ENC] generating IKE_INTERMEDIATE request 1 [ KE ]
...
[IKE] IKE_SA pqgm-ikev2[1] established
```

### 4.5 检查 SA 状态

```bash
# 查看 IKE SA
sudo swanctl --list-sas

# 查看 IPsec SA
sudo ip xfrm state
sudo ip xfrm policy
```

---

## 5. 网络连通性测试

### 5.1 配置虚拟 IP

**Initiator**:
```bash
sudo ip addr add 10.1.0.1/16 dev lo
```

**Responder**:
```bash
sudo ip addr add 10.2.0.1/16 dev lo
```

### 5.2 Ping 测试

```bash
# 在 Initiator 上
ping 10.2.0.1
```

---

## 6. 抓包验证

### 6.1 抓包命令

```bash
# 在任意机器上
sudo tcpdump -i any port 500 or port 4500 -w pqgm_ikev2.pcap
```

### 6.2 预期协议流程

```
1. IKE_SA_INIT (2 packets)
   - 协商 x25519 + ML-KEM-768 + SM2-KEM

2. IKE_INTERMEDIATE #0 (2 packets)
   - 证书分发 (SignCert + EncCert)

3. IKE_INTERMEDIATE #1 (2 packets)
   - SM2-KEM 密钥交换

4. IKE_INTERMEDIATE #2 (2 packets)
   - ML-KEM-768 密钥交换

5. IKE_AUTH (2 packets)
   - 签名认证
```

---

## 7. 故障排查

### 7.1 查看日志

```bash
# 实时日志
journalctl -f

# 或查看 charon 输出
# 如果用 --debug 启动
```

### 7.2 常见问题

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| NO_PROPOSAL_CHOSEN | gmalg 插件未加载 | 检查 `gmalg.conf` |
| CERTIFICATE验证失败 | 证书不匹配 | 检查证书 ID |
| TIMEOUT | 网络不通 | 检查防火墙 |

### 7.3 重启服务

```bash
sudo pkill charon
sleep 2
sudo /usr/local/libexec/ipsec/charon &
sleep 3
sudo swanctl --load-all
```

---

## 8. 快速命令参考

### Initiator

```bash
# 一键配置
sudo /usr/local/libexec/ipsec/charon --debug-ike 2 &
sleep 3
sudo swanctl --load-all
sudo swanctl --initiate --child ipsec
```

### Responder

```bash
# 一键配置
sudo /usr/local/libexec/ipsec/charon --debug-ike 2 &
sleep 3
sudo swanctl --load-all
# 等待 initiator 连接
```

---

## 9. 预期测试结果

- **IKE_SA 建立时间**: ~50-100ms
- **RTT 数量**: 5 (完整 PQ-GM-IKEv2)
- **IPsec SA**: 成功建立
- **数据通道**: 可 ping 通

---

**如有问题，请检查**:
1. 两台机器网络互通
2. 证书正确部署
3. `accept_private_algs = yes` 配置
4. gmalg 和 ml 插件加载
