#!/bin/bash
# 抓包分析脚本 - 捕获 IKEv2 握手报文

PASSWORD="1574a"
INTERFACE="ens33"
CAPTURE_DIR="/home/ipsec/pqgm-test/captures"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p "$CAPTURE_DIR"

echo "=========================================="
echo "  IKEv2 报文捕获分析"
echo "=========================================="
echo ""
echo "接口: $INTERFACE"
echo "保存目录: $CAPTURE_DIR"
echo ""

# 函数：启动抓包并测试连接
capture_connection() {
    local conn_name=$1
    local output_file="$CAPTURE_DIR/${conn_name}_${TIMESTAMP}.pcap"

    echo "捕获连接: $conn_name"
    echo "输出文件: $output_file"
    echo "------------------------"

    # 后台启动 tcpdump
    echo '1574a' | sudo -S tcpdump -i $INTERFACE -w "$output_file" "port 500 or port 4500" &
    tcpdump_pid=$!

    # 等待 tcpdump 启动
    sleep 1

    # 发起连接
    echo '1574a' | sudo -S swanctl --initiate --child "$conn_name"

    # 等待连接建立
    sleep 2

    # 停止 tcpdump
    echo '1574a' | sudo -S kill $tcpdump_pid 2>/dev/null
    wait $tcpdump_pid 2>/dev/null

    # 分析报文
    echo ""
    echo "报文分析:"
    echo '1574a' | sudo -S tcpdump -r "$output_file" -v 2>/dev/null | head -50

    # 统计报文数量和大小
    echo ""
    echo "报文统计:"
    echo '1574a' | sudo -S tcpdump -r "$output_file" -q 2>/dev/null | wc -l | xargs echo "  总报文数:"
    echo '1574a' | sudo -S ls -lh "$output_file" | awk '{print "  文件大小: " $5}'

    # 显示各阶段报文
    echo ""
    echo "IKE_SA_INIT 阶段:"
    echo '1574a' | sudo -S tcpdump -r "$output_file" -vv 2>/dev/null | grep -E "IKE_SA_INIT|isakmp.*sa" | head -5

    echo ""
    echo "IKE_AUTH 阶段:"
    echo '1574a' | sudo -S tcpdump -r "$output_file" -vv 2>/dev/null | grep -E "IKE_AUTH|isakmp.*auth" | head -5

    # 终止连接
    echo '1574a' | sudo -S swanctl --terminate --ike "$conn_name" >/dev/null 2>&1

    echo "------------------------"
    echo ""
}

# 检查接口
if ! ip link show "$INTERFACE" >/dev/null 2>&1; then
    echo "错误: 接口 $INTERFACE 不存在"
    echo "可用接口:"
    ip link show | grep "^[0-9]" | awk '{print "  " $2}' | tr -d ':'
    exit 1
fi

# 捕获基线连接
echo "1. 传统 IKEv2 基线捕获 (x25519)"
echo ""
capture_connection "baseline"

# 捕获混合密钥交换连接
echo "2. 混合密钥交换捕获 (x25519 + ML-KEM-768)"
echo ""
capture_connection "pqgm-hybrid"

echo "=========================================="
echo "  捕获完成"
echo "=========================================="
echo ""
echo "文件保存在: $CAPTURE_DIR"
echo ""
echo "使用 Wireshark 打开:"
echo "  wireshark $CAPTURE_DIR/baseline_${TIMESTAMP}.pcap"
echo "  wireshark $CAPTURE_DIR/pqgm-hybrid_${TIMESTAMP}.pcap"
