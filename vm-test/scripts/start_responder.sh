#!/bin/bash
# Responder启动脚本
# 重启后运行此脚本启动strongSwan服务

set -e

echo "========================================"
echo "Responder 启动脚本"
echo "========================================"

# 检查是否已运行
if pgrep -x charon > /dev/null; then
    echo "[!] charon已在运行，先停止..."
    sudo pkill -x charon || true
    sleep 2
fi

# 清空日志
echo "[1/4] 清空日志..."
sudo truncate -s 0 /var/log/charon.log 2>/dev/null || true

# 启动charon
echo "[2/4] 启动charon..."
sudo /usr/local/libexec/ipsec/charon --debug-ike 2 &
sleep 3

# 加载配置
echo "[3/4] 加载swanctl配置..."
swanctl --load-all

# 验证
echo "[4/4] 验证状态..."
echo ""
echo "=== 连接配置 ==="
swanctl --list-conns | head -20

echo ""
echo "=== 证书 ==="
swanctl --list-certs | head -20

echo ""
echo "=== SM2-KEM预加载状态 ==="
grep -i "sm2" /var/log/charon.log 2>/dev/null || echo "检查日志: tail -f /var/log/charon.log"

echo ""
echo "========================================"
echo "Responder 已启动!"
echo "========================================"
echo "IP: 192.168.172.132"
echo "等待Initiator连接..."
echo ""
echo "常用命令:"
echo "  查看连接: swanctl --list-sas"
echo "  查看日志: tail -f /var/log/charon.log"
echo "  停止服务: sudo pkill charon"
