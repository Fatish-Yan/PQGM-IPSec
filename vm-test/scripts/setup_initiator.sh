#!/bin/bash
# Initiator (VM1) 快速配置脚本
# 在克隆后的机器上运行

set -e

echo "========================================"
echo "Initiator (VM1) 配置脚本"
echo "========================================"

# 检查是否以root运行或使用sudo
if [ "$EUID" -ne 0 ]; then
    echo "请使用 sudo 运行此脚本"
    exit 1
fi

BASE_DIR=/home/ipsec/PQGM-IPSec
VM_TEST_DIR=$BASE_DIR/vm-test
DOCKER_INITIATOR=$BASE_DIR/docker/initiator
SWANCTL_DIR=/usr/local/etc/swanctl

# 新IP地址
NEW_IP=192.168.172.134
RESPONDER_IP=192.168.172.132

echo "[1/8] 修改网络配置..."
# 获取当前连接名称
CONNECTION=$(nmcli -t -f NAME con show --active | head -1)
echo "  当前连接: $CONNECTION"

# 获取当前IP
CURRENT_IP=$(ip addr show | grep -oP 'inet 192\.168\.172\.\K\d+' | head -1)
if [ "$CURRENT_IP" = "133" ]; then
    echo "  IP已经是 .134，跳过"
else
    echo "  修改IP从 .132 到 .134..."
    nmcli con mod "$CONNECTION" ipv4.addresses "${NEW_IP}/24"
    nmcli con mod "$CONNECTION" ipv4.method manual
    nmcli con up "$CONNECTION"
    echo "  IP已修改为 $NEW_IP"
fi

echo "[2/8] 修改主机名..."
hostnamectl set-hostname initiator.pqgm.test
echo "  主机名已设置为 initiator.pqgm.test"

echo "[3/8] 备份现有配置..."
if [ -f "$SWANCTL_DIR/swanctl.conf" ]; then
    cp "$SWANCTL_DIR/swanctl.conf" "$SWANCTL_DIR/swanctl.conf.bak.$(date +%Y%m%d_%H%M%S)"
fi

echo "[4/8] 复制swanctl配置..."
cp "$VM_TEST_DIR/initiator/swanctl.conf" "$SWANCTL_DIR/swanctl.conf"

echo "[5/9] 复制strongswan配置..."
cp "$VM_TEST_DIR/strongswan.conf" /usr/local/etc/strongswan.conf

echo "[6/9] 复制gmalg插件配置 (SM2-KEM预加载)..."
cp "$VM_TEST_DIR/gmalg.conf" /usr/local/etc/strongswan.d/charon/gmalg.conf

echo "[7/9] 复制证书..."
cp "$DOCKER_INITIATOR/certs/x509/"* "$SWANCTL_DIR/x509/" 2>/dev/null || true
cp "$DOCKER_INITIATOR/certs/private/"* "$SWANCTL_DIR/private/" 2>/dev/null || true
cp "$DOCKER_INITIATOR/certs/x509ca/"* "$SWANCTL_DIR/x509ca/" 2>/dev/null || true
cp "$DOCKER_INITIATOR/certs/mldsa/"* "$SWANCTL_DIR/x509/" 2>/dev/null || true

echo "[8/9] 设置权限..."
chmod 600 "$SWANCTL_DIR/private/"*

echo "[9/9] 配置防火墙和hosts..."
if command -v ufw &> /dev/null; then
    ufw allow 500/udp
    ufw allow 4500/udp
    ufw allow esp
    echo "  UFW规则已添加"
fi

# 清理旧的hosts条目（如果存在）
sed -i '/responder.pqgm.test/d' /etc/hosts
sed -i '/initiator.pqgm.test/d' /etc/hosts

# 添加新的hosts条目
echo "$RESPONDER_IP  responder.pqgm.test" >> /etc/hosts
echo "$NEW_IP  initiator.pqgm.test" >> /etc/hosts
echo "  hosts已配置"

echo ""
echo "========================================"
echo "Initiator 配置完成!"
echo "========================================"
echo ""
echo "当前配置:"
echo "  IP: $NEW_IP"
echo "  主机名: initiator.pqgm.test"
echo ""
echo "下一步:"
echo "1. 验证网络: ping $RESPONDER_IP"
echo "2. 清空日志: sudo truncate -s 0 /var/log/charon.log"
echo "3. 重启服务: sudo systemctl restart strongswan"
echo "4. 加载配置: swanctl --load-all"
echo "5. 检查证书: swanctl --list-certs"
echo "6. 发起连接: swanctl --initiate --child net --ike pqgm-5rtt-mldsa"
echo ""
