#!/bin/bash
# Initiator启动脚本
# 克隆VM后在Initiator上运行此脚本

set -e

echo "========================================"
echo "Initiator 启动脚本"
echo "========================================"

# 检查网络
echo "[0/5] 检查网络..."
LOCAL_IP=$(ip addr show | grep -oP 'inet 192\.168\.172\.\K\d+' | head -1)
echo "本机IP: 192.168.172.$LOCAL_IP"

if [ "$LOCAL_IP" != "134" ]; then
    echo "[!] 警告: IP不是192.168.172.134，请先运行setup_initiator.sh"
fi

# 检查是否已运行
if pgrep -x charon > /dev/null; then
    echo "[!] charon已在运行，先停止..."
    sudo pkill -x charon || true
    sleep 2
fi

# 清空日志
echo "[1/5] 清空日志..."
sudo truncate -s 0 /var/log/charon.log 2>/dev/null || true

# 启动charon
echo "[2/5] 启动charon..."
sudo /usr/local/libexec/ipsec/charon --debug-ike 2 &
sleep 3

# 加载配置
echo "[3/5] 加载swanctl配置..."
swanctl --load-all

# 验证
echo "[4/5] 验证状态..."
echo ""
echo "=== 连接配置 ==="
swanctl --list-conns | head -20

echo ""
echo "=== 证书 ==="
swanctl --list-certs | head -20

echo ""
echo "=== SM2-KEM预加载状态 ==="
grep -i "sm2" /var/log/charon.log 2>/dev/null || echo "检查日志: tail -f /var/log/charon.log"

# 测试网络连通性
echo ""
echo "[5/5] 测试网络连通性..."
ping -c 2 192.168.172.132

echo ""
echo "========================================"
echo "Initiator 已启动!"
echo "========================================"
echo "IP: 192.168.172.134"
echo "对端: 192.168.172.132 (Responder)"
echo ""
echo "发起连接命令:"
echo "  swanctl --initiate --child net --ike pqgm-5rtt-mldsa"
echo ""
echo "完整测试 (IKE+ESP):"
echo "  ./run_full_test.sh pqgm-5rtt-mldsa 5"
