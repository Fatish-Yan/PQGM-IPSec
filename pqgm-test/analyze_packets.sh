#!/bin/bash
# 报文分析脚本 - 详细分析各阶段报文大小

PASSWORD="1574a"
RESULTS_DIR="/home/ipsec/pqgm-test/results"
INTERFACE="ens33"

mkdir -p "$RESULTS_DIR"

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "=========================================="
echo "  通信开销分析 - 报文大小统计"
echo "=========================================="
echo ""

# 函数：捕获并分析单个连接
analyze_connection() {
    local conn_name=$1
    local conn_desc=$2

    echo "分析配置: $conn_desc ($conn_name)"
    echo "------------------------"

    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    pcap_file="$RESULTS_DIR/${conn_name}_packets_${TIMESTAMP}.pcap"

    # 确保没有现有连接
    echo "$PASSWORD" | sudo -S swanctl --terminate --ike $conn_name >/dev/null 2>&1
    sleep 2

    # 启动抓包
    echo "$PASSWORD" | sudo -S tcpdump -i $INTERFACE -w "$pcap_file" "port 500 or port 4500" 2>/dev/null &
    tcpdump_pid=$!
    sleep 2

    # 发起连接
    echo "发起连接..."
    echo "$PASSWORD" | sudo -S swanctl --initiate --ike $conn_name >/dev/null 2>&1

    # 等待连接建立
    sleep 3

    # 停止抓包
    echo "$PASSWORD" | sudo -S kill $tcpdump_pid 2>/dev/null
    wait $tcpdump_pid 2>/dev/null
    sleep 1

    # 终止连接
    echo "$PASSWORD" | sudo -S swanctl --terminate --ike $conn_name >/dev/null 2>&1
    sleep 2

    # 分析报文
    echo ""
    echo "报文统计:"

    # 获取总报文数和总字节数
    packet_info=$(echo "$PASSWORD" | sudo -S tcpdump -r "$pcap_file" -nn 2>/dev/null)
    packet_count=$(echo "$packet_info" | wc -l)

    # 计算总字节数（使用 tshark 或 tcpdump）
    total_bytes=0
    packet_sizes=()

    while IFS= read -r line; do
        if echo "$line" | grep -q "length"; then
            # 提取长度值
            size=$(echo "$line" | grep -oP 'length \d+' | awk '{print $2}')
            if [ -n "$size" ]; then
                # IP包长度包含头部，UDP数据包需要减去IP+UDP头部(28字节)
                # 但这是链路层长度，直接使用即可
                total_bytes=$((total_bytes + size))
                packet_sizes+=($size)
            fi
        fi
    done <<< "$packet_info"

    file_size=$(echo "$PASSWORD" | sudo -S ls -lh "$pcap_file" | awk '{print $5}')

    echo "  总报文数: $packet_count"
    echo "  总字节数: $total_bytes"
    echo "  抓包文件: $file_size"

    # 详细报文列表
    echo ""
    echo "详细报文列表:"
    echo "$PASSWORD" | sudo -S tcpdump -r "$pcap_file" -nn -v 2>/dev/null | grep -E "IP.*udp|length" | head -20

    # 保存详细数据
    echo "$packet_info" > "$RESULTS_DIR/${conn_name}_packet_detail.txt"

    echo ""
    echo -e "${GREEN}✓ 分析完成${NC}"
    echo ""
}

# 分析基线连接
analyze_connection "baseline" "传统 IKEv2 (x25519)"

# 分析混合密钥交换
analyze_connection "pqgm-hybrid" "混合密钥交换 (x25519 + ML-KEM-768)"

echo "=========================================="
echo "  分析完成"
echo "=========================================="
echo ""
echo "详细数据保存在: $RESULTS_DIR"

# 生成对比报告
echo ""
echo "=========================================="
echo "通信开销对比"
echo "=========================================="
echo ""

for conn in "baseline" "pqgm-hybrid"; do
    if [ -f "$RESULTS_DIR/${conn}_packet_detail.txt" ]; then
        echo "$conn 配置:"
        echo "----------------------------------------"
        count=$(wc -l < "$RESULTS_DIR/${conn}_packet_detail.txt")
        echo "  报文数量: $count"

        # 计算总字节数
        total=0
        while IFS= read -r line; do
            size=$(echo "$line" | grep -oP 'length \d+' | awk '{print $2}')
            if [ -n "$size" ]; then
                total=$((total + size))
            fi
        done < "$RESULTS_DIR/${conn}_packet_detail.txt"
        echo "  总字节数: $total"
        echo "  平均每报文: $((total / count)) 字节"
        echo ""
    fi
done
