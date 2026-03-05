#!/bin/bash
# 日志收集脚本

OUTPUT_DIR=/home/ipsec/PQGM-IPSec/vm-test/results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# 自动检测角色
INTERFACE=$(ip route | grep default | awk '{print $5}' | head -1)
LOCAL_IP=$(ip addr show "$INTERFACE" | grep -oP 'inet \K[\d.]+')
if [[ "$LOCAL_IP" == *"132"* ]]; then
    ROLE="responder"
else
    ROLE="initiator"
fi

COLLECT_DIR="${OUTPUT_DIR}/collect_${ROLE}_${TIMESTAMP}"
mkdir -p "$COLLECT_DIR"

echo "========================================"
echo "日志收集"
echo "========================================"
echo "角色: $ROLE"
echo "输出目录: $COLLECT_DIR"
echo "========================================"

# 收集charon日志
if [ -f /var/log/charon.log ]; then
    cp /var/log/charon.log "${COLLECT_DIR}/charon.log"
    echo "[+] charon.log"
fi

# 收集syslog中的strongswan日志
if [ -f /var/log/syslog ]; then
    grep -i "charon\|strongswan\|ike" /var/log/syslog > "${COLLECT_DIR}/syslog_filtered.log" 2>/dev/null || true
    echo "[+] syslog (filtered)"
fi

# 收集swanctl状态
swanctl --list-sas > "${COLLECT_DIR}/sas.txt" 2>&1 || echo "[!] 无法获取SA列表"
swanctl --list-certs > "${COLLECT_DIR}/certs.txt" 2>&1 || echo "[!] 无法获取证书列表"
swanctl --list-pools > "${COLLECT_DIR}/pools.txt" 2>&1 || true

# 收集网络配置
ip addr show > "${COLLECT_DIR}/ip_addr.txt"
ip route show > "${COLLECT_DIR}/ip_route.txt"

# 收集strongswan配置
if [ -f /usr/local/etc/swanctl/swanctl.conf ]; then
    cp /usr/local/etc/swanctl/swanctl.conf "${COLLECT_DIR}/"
fi
if [ -f /usr/local/etc/strongswan.conf ]; then
    cp /usr/local/etc/strongswan.conf "${COLLECT_DIR}/"
fi

# 打包
cd "$OUTPUT_DIR"
tar -czvf "logs_${ROLE}_${TIMESTAMP}.tar.gz" "collect_${ROLE}_${TIMESTAMP}"
rm -rf "collect_${ROLE}_${TIMESTAMP}"

echo ""
echo "[*] 已打包: ${OUTPUT_DIR}/logs_${ROLE}_${TIMESTAMP}.tar.gz"
