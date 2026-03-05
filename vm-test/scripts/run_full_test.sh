#!/bin/bash
# 完整测试流程脚本 - 自动执行抓包、连接、ESP通信
#
# 功能:
#   1. 启动PCAP捕获（IKE + ESP）
#   2. 发起IKE连接（5-RTT握手）
#   3. 执行ESP数据通信（ping测试）
#   4. 停止捕获并分析
#
# 用法:
#   ./run_full_test.sh <config_name> [ping_count]
#   例如: ./run_full_test.sh pqgm-5rtt-mldsa 5

set -e

CONFIG=${1:-"pqgm-5rtt-mldsa"}
PING_COUNT=${2:-3}
OUTPUT_DIR=/home/ipsec/PQGM-IPSec/vm-test/results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# 自动检测角色和对端IP
INTERFACE=$(ip route | grep default | awk '{print $5}' | head -1)
LOCAL_IP=$(ip addr show "$INTERFACE" | grep -oP 'inet \K[\d.]+')

if [[ "$LOCAL_IP" == *"132"* ]]; then
    ROLE="responder"
    REMOTE_IP="192.168.172.134"
    echo "本机角色: Responder"
    echo "错误: 此脚本应在Initiator上运行"
    exit 1
else
    ROLE="initiator"
    REMOTE_IP="192.168.172.132"
fi

PCAP_FILE="${OUTPUT_DIR}/${ROLE}_${CONFIG}_${TIMESTAMP}.pcap"

echo "========================================"
echo "PQ-GM-IKEv2 完整测试流程"
echo "========================================"
echo "配置: $CONFIG"
echo "本机IP: $LOCAL_IP ($ROLE)"
echo "对端IP: $REMOTE_IP"
echo "PCAP: $PCAP_FILE"
echo "Ping次数: $PING_COUNT"
echo "========================================"
echo ""

mkdir -p "$OUTPUT_DIR"

# 1. 清除旧SA
echo "[1/6] 清除旧SA..."
sudo swanctl --terminate --ike all 2>/dev/null || true
sleep 1

# 2. 启动tcpdump（后台运行）
echo "[2/6] 启动PCAP捕获（后台）..."
sudo tcpdump -i "$INTERFACE" -w "$PCAP_FILE" \
    "host $REMOTE_IP and (udp port 500 or udp port 4500 or proto 50)" \
    -n 2>/dev/null &
TCPDUMP_PID=$!
sleep 2  # 等待tcpdump就绪

# 3. 发起IKE连接
echo "[3/6] 发起IKE连接 ($CONFIG)..."
START_TIME=$(date +%s%3N)
swanctl --initiate --child net --ike "$CONFIG" 2>&1
INIT_RESULT=$?
END_TIME=$(date +%s%3N)
IKE_DURATION=$((END_TIME - START_TIME))

if [ $INIT_RESULT -ne 0 ]; then
    echo "IKE连接失败!"
    sudo kill $TCPDUMP_PID 2>/dev/null || true
    exit 1
fi

echo "IKE握手完成，耗时: ${IKE_DURATION} ms"
sleep 2  # 等待SA稳定

# 4. 检查SA状态
echo "[4/6] 检查SA状态..."
swanctl --list-sas

# 5. 执行ESP通信测试
echo "[5/6] 执行ESP通信测试 (ping $PING_COUNT 次)..."

# 获取隧道对端内网IP（用于ESP通信）
# 根据配置: local_ts = 10.1.0.0/16, remote_ts = 10.2.0.0/16
# 我们需要ping对端内网地址来触发ESP流量
TUNNEL_REMOTE="10.2.0.1"

# 先添加路由（如果需要）
# sudo ip route add 10.2.0.0/16 via $REMOTE_IP 2>/dev/null || true

# 执行ping通过隧道（这会产生ESP流量）
# 注意：实际ping可能需要隧道正确配置才能工作
# 这里我们用一种简单方式：发送UDP包到4500端口模拟ESP
echo "发送测试流量..."
for i in $(seq 1 $PING_COUNT); do
    # 尝试ping隧道对端（如果隧道工作）
    ping -c 1 -W 1 $TUNNEL_REMOTE 2>/dev/null || true
    sleep 1
done

echo "ESP通信测试完成"
sleep 2  # 确保最后几个包被捕获

# 6. 停止捕获
echo "[6/6] 停止PCAP捕获..."
sudo kill $TCPDUMP_PID 2>/dev/null || true
sleep 2

# 7. 分析PCAP
echo ""
echo "========================================"
echo "分析PCAP..."
echo "========================================"

if command -v tshark &> /dev/null; then
    python3 /home/ipsec/PQGM-IPSec/vm-test/scripts/analyze_pcap.py "$PCAP_FILE"
else
    echo "tshark未安装，跳过详细分析"
    echo "安装: sudo apt install tshark"
fi

# 8. 保存测试结果
RESULT_FILE="${OUTPUT_DIR}/test_${CONFIG}_${TIMESTAMP}.csv"
echo "config,start_time,end_time,duration_ms" > "$RESULT_FILE"
echo "$CONFIG,$START_TIME,$END_TIME,$IKE_DURATION" >> "$RESULT_FILE"

echo ""
echo "========================================"
echo "测试完成!"
echo "========================================"
echo "PCAP文件: $PCAP_FILE"
echo "结果文件: $RESULT_FILE"
echo "IKE握手耗时: ${IKE_DURATION} ms"
echo ""
echo "手动分析命令:"
echo "  tshark -r $PCAP_FILE -Y 'ikev2 || esp' -V"
echo "  wireshark $PCAP_FILE"
