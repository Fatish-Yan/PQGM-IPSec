#!/bin/bash
# PCAP捕获脚本 - 捕获完整IKEv2 + ESP通信流程
#
# 捕获内容:
#   1. IKE_SA_INIT (UDP 500)
#   2. IKE_INTERMEDIATE (UDP 4500, NAT-T封装)
#   3. IKE_AUTH (UDP 4500, NAT-T封装)
#   4. ESP数据通信 (协议50 或 UDP 4500封装)
#
# 使用方法:
#   在Initiator上: ./start_capture.sh 192.168.172.132
#   在Responder上: ./start_capture.sh 192.168.172.134

OUTPUT_DIR=/home/ipsec/PQGM-IPSec/vm-test/results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
INTERFACE=$(ip route | grep default | awk '{print $5}' | head -1)
REMOTE_IP=${1:-"192.168.172.132"}

# 自动检测本地角色
LOCAL_IP=$(ip addr show "$INTERFACE" | grep -oP 'inet \K[\d.]+')
if [[ "$LOCAL_IP" == *"132"* ]]; then
    ROLE="responder"
else
    ROLE="initiator"
fi

OUTPUT="${OUTPUT_DIR}/${ROLE}_${TIMESTAMP}.pcap"

echo "========================================"
echo "PCAP 捕获 - 完整IKEv2+ESP流程"
echo "========================================"
echo "接口: $INTERFACE"
echo "本地IP: $LOCAL_IP"
echo "对端IP: $REMOTE_IP"
echo "角色: $ROLE"
echo "输出: $OUTPUT"
echo ""
echo "捕获范围:"
echo "  - IKE_SA_INIT (UDP 500)"
echo "  - IKE_INTERMEDIATE (UDP 4500)"
echo "  - IKE_AUTH (UDP 4500)"
echo "  - ESP通信 (协议50 + UDP 4500封装)"
echo "========================================"

mkdir -p "$OUTPUT_DIR"

echo "[*] 开始捕获... (Ctrl+C 停止)"
echo ""

# 捕获过滤器说明:
# - UDP 500: IKE初始交换（未封装）
# - UDP 4500: NAT-T封装的IKE和ESP
# - proto 50: 原生ESP协议（非NAT-T场景）
# - host过滤: 只捕获与对端的通信
sudo tcpdump -i "$INTERFACE" -w "$OUTPUT" \
    "host $REMOTE_IP and (udp port 500 or udp port 4500 or proto 50)" \
    -v

echo ""
echo "[*] 捕获已保存到: $OUTPUT"
echo ""
echo "分析命令:"
echo "  tshark -r $OUTPUT -Y 'ikev2 || esp' -V"
echo "  python3 /home/ipsec/PQGM-IPSec/vm-test/scripts/analyze_pcap.py $OUTPUT"
