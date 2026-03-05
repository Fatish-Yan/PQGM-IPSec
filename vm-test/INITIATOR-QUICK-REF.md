# PQ-GM-IKEv2 Initiator VM 快速参考

## 当前角色
你是 **Initiator (发起方)** - 负责发起 IKEv2 连接请求

## 网络配置
| 角色 | IP | 主机名 |
|------|-----|--------|
| **Initiator (本机)** | 192.168.172.134 | initiator.pqgm.test |
| Responder (对端) | 192.168.172.132 | responder.pqgm.test |

## 快速配置命令

```bash
# 一键配置
sudo /home/ipsec/PQGM-IPSec/vm-test/scripts/setup_initiator_vm.sh

# 或手动执行关键命令
sudo hostnamectl set-hostname initiator.pqgm.test
sudo nmcli con mod "Wired connection 1" ipv4.addresses 192.168.172.134/24
sudo nmcli con up "Wired connection 1"
```

## 测试流程

```bash
# 1. 加载配置
swanctl --load-all

# 2. 发起连接 (标准算法)
swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 3. 或发起连接 (国密对称栈)
swanctl --initiate --child net --ike pqgm-5rtt-gm-symm

# 4. 查看 SA 状态
swanctl --list-sas
```

## 关键文件

| 文件 | 路径 |
|------|------|
| swanctl.conf | `/usr/local/etc/swanctl/swanctl.conf` |
| ML-DSA 证书 | `/usr/local/etc/swanctl/x509/initiator_hybrid_cert.pem` |
| ML-DSA 私钥 | `/usr/local/etc/swanctl/private/initiator_mldsa_key.bin` |
| SM2 私钥 | `/usr/local/etc/swanctl/private/enc_key.pem` |

## 成功标志

看到以下日志表示连接成功：
```
[IKE] IKE_SA pqgm-5rtt-mldsa[1] established
[IKE] CHILD_SA net{1} established
initiate completed successfully
```

## 详细文档

完整配置指南: `/home/ipsec/PQGM-IPSec/vm-test/INITIATOR-SETUP-GUIDE.md`
