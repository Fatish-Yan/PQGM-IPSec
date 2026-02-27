# 使用 Network Namespace 模拟双机测试

> 创建时间: 2026-02-27
> 目的: 绕过本地回环的 ID=%any 问题，验证完整的 5-RTT PQ-GM-IKEv2 流程

---

## 1. 方案概述

使用 Linux network namespace 创建两个隔离的网络环境：
- **ns-initiator**: 模拟 Initiator (192.168.100.10)
- **ns-responder**: 模拟 Responder (192.168.100.20)

---

## 2. 快速测试脚本

```bash
#!/bin/bash
# /home/ipsec/PQGM-IPSec/scripts/ns-test.sh

set -e

# 配置
NS_INIT=ns-init
NS_RESP=ns-resp
VETH_INIT=veth-init
VETH_RESP=veth-resp
IP_INIT=192.168.100.10
IP_RESP=192.168.100.20

echo "=== 1. 创建 network namespace ==="
# 清理旧的
sudo ip netns del $NS_INIT 2>/dev/null || true
sudo ip netns del $NS_RESP 2>/dev/null || true
sudo ip link del $VETH_INIT 2>/dev/null || true

# 创建新的
sudo ip netns add $NS_INIT
sudo ip netns add $NS_RESP

# 创建 veth pair
sudo ip link add $VETH_INIT type veth peer name $VETH_RESP

# 将 veth 分配到 namespace
sudo ip link set $VETH_INIT netns $NS_INIT
sudo ip link set $VETH_RESP netns $NS_RESP

# 配置 IP 地址
sudo ip netns exec $NS_INIT ip addr add $IP_INIT/24 dev $VETH_INIT
sudo ip netns exec $NS_INIT ip link set $VETH_INIT up
sudo ip netns exec $NS_INIT ip link set lo up

sudo ip netns exec $NS_RESP ip addr add $IP_RESP/24 dev $VETH_RESP
sudo ip netns exec $NS_RESP ip link set $VETH_RESP up
sudo ip netns exec $NS_RESP ip link set lo up

echo "=== 2. 测试连通性 ==="
sudo ip netns exec $NS_INIT ping -c 2 $IP_RESP

echo "=== 3. 准备证书和配置 ==="
# 复制证书到 namespace 隔离目录
mkdir -p /tmp/ns-test/{initiator,responder}

# CA 证书
cp /home/ipsec/PQGM-IPSec/certs-pqgm/ca/caCert.pem /tmp/ns-test/

# Initiator 证书
cp /home/ipsec/PQGM-IPSec/certs-pqgm/initiator/signCert.pem /tmp/ns-test/initiator/
cp /home/ipsec/PQGM-IPSec/certs-pqgm/initiator/encCert.pem /tmp/ns-test/initiator/
cp /home/ipsec/PQGM-IPSec/certs-pqgm/initiator/signKey.pem /tmp/ns-test/initiator/
cp /home/ipsec/PQGM-IPSec/certs-pqgm/initiator/encKey.pem /tmp/ns-test/initiator/

# Responder 证书
cp /home/ipsec/PQGM-IPSec/certs-pqgm/responder/signCert.pem /tmp/ns-test/responder/
cp /home/ipsec/PQGM-IPSec/certs-pqgm/responder/encCert.pem /tmp/ns-test/responder/
cp /home/ipsec/PQGM-IPSec/certs-pqgm/responder/signKey.pem /tmp/ns-test/responder/
cp /home/ipsec/PQGM-IPSec/certs-pqgm/responder/encKey.pem /tmp/ns-test/responder/

echo "=== 4. 创建配置文件 ==="
# Initiator 配置
cat > /tmp/ns-test/initiator/swanctl.conf << 'EOF'
connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.100.10
        remote_addrs = 192.168.100.20
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
            }
        }
    }
}

secrets {
    sign { file = signKey.pem }
    enc { file = encKey.pem }
}
EOF

# Responder 配置
cat > /tmp/ns-test/responder/swanctl.conf << 'EOF'
connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.100.20
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
            }
        }
    }
}

secrets {
    sign { file = signKey.pem }
    enc { file = encKey.pem }
}
EOF

echo "=== 5. 启动 Responder ==="
# 在 responder namespace 中启动 charon
sudo ip netns exec $NS_RESP /usr/local/libexec/ipsec/charon --debug-ike 2 &
RESP_PID=$!
sleep 3

# 加载配置
sudo ip netns exec $NS_RESP /usr/local/sbin/swanctl --load-all

echo "=== 6. 启动 Initiator 并测试 ==="
# 在 initiator namespace 中启动 charon
sudo ip netns exec $NS_INIT /usr/local/libexec/ipsec/charon --debug-ike 2 &
INIT_PID=$!
sleep 3

# 加载配置
sudo ip netns exec $NS_INIT /usr/local/sbin/swanctl --load-all

# 发起连接
sudo ip netns exec $NS_INIT /usr/local/sbin/swanctl --initiate --child ipsec

echo "=== 7. 检查结果 ==="
sudo ip netns exec $NS_INIT /usr/local/sbin/swanctl --list-sas

echo "=== 测试完成 ==="
echo "Responder PID: $RESP_PID"
echo "Initiator PID: $INIT_PID"
echo ""
echo "清理命令:"
echo "  sudo ip netns del $NS_INIT"
echo "  sudo ip netns del $NS_RESP"
```

---

## 3. 执行步骤

```bash
# 1. 创建脚本
mkdir -p /home/ipsec/PQGM-IPSec/scripts
# (将上面的脚本保存到 scripts/ns-test.sh)

# 2. 赋予执行权限
chmod +x /home/ipsec/PQGM-IPSec/scripts/ns-test.sh

# 3. 执行测试
sudo /home/ipsec/PQGM-IPSec/scripts/ns-test.sh
```

---

## 4. 预期结果

如果成功，应该看到：

```
[CFG] selected proposal: .../KE1_ML_KEM_768/KE2_SM2KEM
[IKE] SM2-KEM: injecting IDs - peer=responder-sign.pqgm.test, my=initiator-sign.pqgm.test
[IKE] SM2-KEM: EncCert found for ...
[IKE] IKE_SA established
```

---

## 5. 注意事项

1. 需要在每个 namespace 中运行独立的 charon 实例
2. 证书文件路径需要正确配置
3. 清理测试环境后才能再次运行
