#!/bin/bash
# ============================================================================
# PQ-GM-IKEv2 Initiator VM 配置脚本
#
# 用途: 将克隆的虚拟机配置为 Initiator (发起方)
#
# 使用方法:
#   chmod +x setup_initiator_vm.sh
#   sudo ./setup_initiator_vm.sh
#
# 前提条件:
#   1. 已从 Responder VM 克隆此虚拟机
#   2. VM 网络已配置为同一网段
#   3. /home/ipsec/PQGM-IPSec 目录存在
#
# 作者: PQ-GM-IKEv2 项目
# 日期: 2026-03-05
# ============================================================================

set -e  # 遇到错误立即退出

# 配置变量 (根据实际环境修改)
# ============================================
RESPONDER_IP="192.168.172.132"      # Responder (原机器) IP
INITIATOR_IP="192.168.172.134"      # Initiator (本机) 新 IP
INITIATOR_HOSTNAME="initiator.pqgm.test"

# 项目路径
PROJECT_DIR="/home/ipsec/PQGM-IPSec"
DOCKER_INITIATOR="${PROJECT_DIR}/docker/initiator/certs"
VMTEST_INITIATOR="${PROJECT_DIR}/vm-test/initiator"
SWANCTL_DIR="/usr/local/etc/swanctl"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查是否以 root 运行
if [ "$EUID" -ne 0 ]; then
    log_error "请使用 sudo 运行此脚本"
    exit 1
fi

echo "=========================================="
echo "  PQ-GM-IKEv2 Initiator VM 配置脚本"
echo "=========================================="
echo ""
echo "Responder IP: ${RESPONDER_IP}"
echo "Initiator IP: ${INITIATOR_IP}"
echo "Initiator 主机名: ${INITIATOR_HOSTNAME}"
echo ""

read -p "确认以上配置正确? (y/n): " confirm
if [ "$confirm" != "y" ]; then
    echo "已取消"
    exit 0
fi

# ============================================
# 步骤 1: 修改主机名
# ============================================
log_info "步骤 1/7: 修改主机名..."
hostnamectl set-hostname "$INITIATOR_HOSTNAME"
log_info "主机名已修改为: $INITIATOR_HOSTNAME"

# ============================================
# 步骤 2: 配置 IP 地址
# ============================================
log_info "步骤 2/7: 配置 IP 地址..."

# 获取当前网络连接名称
CONNECTION=$(nmcli -t -f NAME con show --active | head -1)

if [ -z "$CONNECTION" ]; then
    log_error "无法找到活动的网络连接"
    exit 1
fi

log_info "找到网络连接: $CONNECTION"
log_warn "将修改 IP 地址为: $INITIATOR_IP"

# 修改 IP 地址
nmcli con mod "$CONNECTION" ipv4.addresses "${INITIATOR_IP}/24"
nmcli con mod "$CONNECTION" ipv4.method manual
nmcli con up "$CONNECTION"

log_info "IP 地址已修改"

# ============================================
# 步骤 3: 修改 /etc/hosts
# ============================================
log_info "步骤 3/7: 修改 /etc/hosts..."

# 移除旧的 initiator 映射 (如果有)
sed -i '/initiator\.pqgm\.test/d' /etc/hosts
sed -i '/responder\.pqgm\.test/d' /etc/hosts

# 添加新的映射
echo "${RESPONDER_IP}    responder.pqgm.test" >> /etc/hosts
echo "${INITIATOR_IP}    initiator.pqgm.test" >> /etc/hosts

log_info "/etc/hosts 已更新"

# ============================================
# 步骤 4: 复制 Initiator 证书
# ============================================
log_info "步骤 4/7: 复制 Initiator 证书..."

# ML-DSA 混合证书
cp "${DOCKER_INITIATOR}/x509/initiator_hybrid_cert.pem" "${SWANCTL_DIR}/x509/"
log_info "  - initiator_hybrid_cert.pem"

# SM2 证书 (SignCert 和 EncCert)
cp "${DOCKER_INITIATOR}/x509/signCert.pem" "${SWANCTL_DIR}/x509/"
cp "${DOCKER_INITIATOR}/x509/encCert.pem" "${SWANCTL_DIR}/x509/"
log_info "  - signCert.pem, encCert.pem"

# CA 证书 (如果需要更新)
cp "${DOCKER_INITIATOR}/x509ca/mldsa_ca.pem" "${SWANCTL_DIR}/x509ca/"
log_info "  - mldsa_ca.pem"

# ============================================
# 步骤 5: 复制 Initiator 私钥
# ============================================
log_info "步骤 5/7: 复制 Initiator 私钥..."

# ML-DSA 私钥
cp "${DOCKER_INITIATOR}/private/initiator_mldsa_key.bin" "${SWANCTL_DIR}/private/"
chmod 600 "${SWANCTL_DIR}/private/initiator_mldsa_key.bin"
log_info "  - initiator_mldsa_key.bin"

# SM2 加密私钥
cp "${DOCKER_INITIATOR}/private/enc_key.pem" "${SWANCTL_DIR}/private/"
chmod 600 "${SWANCTL_DIR}/private/enc_key.pem"
log_info "  - enc_key.pem"

# ============================================
# 步骤 6: 复制 swanctl.conf 配置
# ============================================
log_info "步骤 6/7: 复制 swanctl.conf 配置..."

# 备份原配置
if [ -f "${SWANCTL_DIR}/swanctl.conf" ]; then
    cp "${SWANCTL_DIR}/swanctl.conf" "${SWANCTL_DIR}/swanctl.conf.bak"
    log_info "  - 原配置已备份到 swanctl.conf.bak"
fi

# 复制 Initiator 配置
cp "${VMTEST_INITIATOR}/swanctl.conf" "${SWANCTL_DIR}/swanctl.conf"
log_info "  - swanctl.conf 已更新"

# ============================================
# 步骤 7: 重启 strongSwan
# ============================================
log_info "步骤 7/7: 重启 strongSwan..."

# 停止可能运行的 charon
pkill charon 2>/dev/null || true

# 重新加载库缓存
ldconfig

# 启动 strongSwan (根据实际情况选择方式)
if systemctl is-enabled strongswan 2>/dev/null; then
    systemctl restart strongswan
    log_info "  - strongSwan 服务已重启"
else
    log_info "  - strongSwan 未作为服务安装，请手动启动 charon"
fi

# ============================================
# 验证配置
# ============================================
echo ""
echo "=========================================="
echo "  配置完成!"
echo "=========================================="
echo ""
echo "验证步骤:"
echo ""
echo "1. 检查主机名:"
echo "   hostname"
echo ""
echo "2. 检查 IP 地址:"
echo "   ip addr"
echo ""
echo "3. 检查网络连通性:"
echo "   ping ${RESPONDER_IP}"
echo ""
echo "4. 加载 strongSwan 配置:"
echo "   swanctl --load-all"
echo ""
echo "5. 发起 5-RTT 连接测试:"
echo "   swanctl --initiate --child net --ike pqgm-5rtt-mldsa"
echo ""
echo "6. 查看 SA 状态:"
echo "   swanctl --list-sas"
echo ""
