#!/bin/bash
# Responder (VM2) 快速配置脚本
# 在当前机器上运行

set -e

echo "========================================"
echo "Responder (VM2) 配置脚本"
echo "========================================"

# 检查是否以root运行或使用sudo
if [ "$EUID" -ne 0 ]; then
    echo "请使用 sudo 运行此脚本"
    exit 1
fi

BASE_DIR=/home/ipsec/PQGM-IPSec
VM_TEST_DIR=$BASE_DIR/vm-test
DOCKER_RESPONDER=$BASE_DIR/docker/responder
SWANCTL_DIR=/usr/local/etc/swanctl

echo "[1/7] 备份现有配置..."
if [ -f "$SWANCTL_DIR/swanctl.conf" ]; then
    cp "$SWANCTL_DIR/swanctl.conf" "$SWANCTL_DIR/swanctl.conf.bak.$(date +%Y%m%d_%H%M%S)"
fi

echo "[2/7] 复制swanctl配置..."
cp "$VM_TEST_DIR/responder/swanctl.conf" "$SWANCTL_DIR/swanctl.conf"

echo "[3/8] 复制strongswan配置..."
cp "$VM_TEST_DIR/strongswan.conf" /usr/local/etc/strongswan.conf

echo "[4/8] 复制gmalg插件配置 (SM2-KEM预加载)..."
cp "$VM_TEST_DIR/gmalg.conf" /usr/local/etc/strongswan.d/charon/gmalg.conf

echo "[5/8] 复制证书..."
cp "$DOCKER_RESPONDER/certs/x509/"* "$SWANCTL_DIR/x509/" 2>/dev/null || true
cp "$DOCKER_RESPONDER/certs/private/"* "$SWANCTL_DIR/private/" 2>/dev/null || true
cp "$DOCKER_RESPONDER/certs/x509ca/"* "$SWANCTL_DIR/x509ca/" 2>/dev/null || true
cp "$DOCKER_RESPONDER/certs/mldsa/"* "$SWANCTL_DIR/x509/" 2>/dev/null || true

echo "[6/8] 设置权限..."
chmod 600 "$SWANCTL_DIR/private/"*

echo "[7/8] 配置防火墙..."
if command -v ufw &> /dev/null; then
    ufw allow 500/udp
    ufw allow 4500/udp
    ufw allow esp
    echo "  UFW规则已添加"
else
    echo "  UFW未安装，跳过防火墙配置"
fi

echo "[8/8] 配置hosts..."
if ! grep -q "initiator.pqgm.test" /etc/hosts; then
    echo "192.168.172.134  initiator.pqgm.test" >> /etc/hosts
    echo "  已添加initiator到hosts"
else
    echo "  hosts已配置"
fi

echo ""
echo "========================================"
echo "Responder 配置完成!"
echo "========================================"
echo ""
echo "下一步:"
echo "1. 清空日志: sudo truncate -s 0 /var/log/charon.log"
echo "2. 重启服务: sudo systemctl restart strongswan"
echo "3. 加载配置: swanctl --load-all"
echo "4. 检查证书: swanctl --list-certs"
echo ""
