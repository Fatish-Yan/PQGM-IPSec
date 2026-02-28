#!/bin/bash
# Network Namespace 双机测试脚本

set -e

# 配置
NS_INIT=ns-init
NS_RESP=ns-resp
VETH_INIT=veth-init
VETH_RESP=veth-resp
IP_INIT=192.168.100.10
IP_RESP=192.168.100.20

echo "=== 清理旧环境 ==="
sudo ip netns del $NS_INIT 2>/dev/null || true
sudo ip netns del $NS_RESP 2>/dev/null || true
sudo ip link del $VETH_INIT 2>/dev/null || true
sudo pkill -f "charon.*ns-" 2>/dev/null || true
sleep 1

echo "=== 1. 创建 network namespace ==="
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

echo "=== Network namespace 创建成功 ==="
echo "Initiator: $NS_INIT ($IP_INIT)"
echo "Responder: $NS_RESP ($IP_RESP)"
