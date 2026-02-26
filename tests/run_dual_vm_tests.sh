#!/bin/bash
# 自动化双VM测试脚本 - 在Initiator上运行

PASSWORD="1574a"
RESPONDER_IP="192.168.172.131"
RESULTS_DIR="/home/ipsec/pqgm-test/results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "  PQGM-IKEv2 自动化测试套件"
echo "=========================================="
echo ""
echo "Initiator IP: $(ip addr show ens33 | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1)"
echo "Responder IP: $RESPONDER_IP"
echo "结果目录: $RESULTS_DIR"
echo ""

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 1. 检查网络连通性
echo "1. 检查网络连通性"
echo "------------------------"
if ping -c 3 -W 2 $RESPONDER_IP >/dev/null 2>&1; then
    echo -e "${GREEN}✓ Responder 可达${NC}"
else
    echo -e "${RED}✗ 无法连接到 Responder ($RESPONDER_IP)${NC}"
    echo "请检查："
    echo "  1. Responder VM 是否已启动"
    echo "  2. IP 地址是否正确"
    exit 1
fi
echo ""

# 2. 检查 Responder 服务状态
echo "2. 检查 Responder 服务"
echo "------------------------"
if nc -zv $RESPONDER_IP 500 2>&1 | grep -q "succeeded"; then
    echo -e "${GREEN}✓ Responder IKE 端口 (500) 开放${NC}"
else
    echo -e "${YELLOW}⚠ Responder IKE 端口可能未开放${NC}"
fi
echo ""

# 3. 确保 charon 运行中
echo "3. 启动 Initiator charon"
echo "------------------------"
echo "$PASSWORD" | sudo -S pkill charon 2>/dev/null
sleep 1
echo "$PASSWORD" | sudo -S /usr/libexec/ipsec/charon &
sleep 2
echo "$PASSWORD" | sudo -S swanctl --load-all >/dev/null 2>&1
echo -e "${GREEN}✓ charon 已启动${NC}"
echo ""

# 4. 测试基线连接 (传统 IKEv2)
echo "=========================================="
echo "4. 测试基线连接 (传统 IKEv2: x25519)"
echo "=========================================="
echo ""

# 启动抓包
echo "$PASSWORD" | sudo -S tcpdump -i ens33 -w "$RESULTS_DIR/baseline_$TIMESTAMP.pcap" "port 500 or port 4500" 2>/dev/null &
tcpdump_pid=$!
sleep 1

# 发起连接
echo "发起连接..."
start_time=$(date +%s%3N)
echo "$PASSWORD" | sudo -S swanctl --initiate --ike baseline 2>&1 | tee "$RESULTS_DIR/baseline_log_$TIMESTAMP.txt"
end_time=$(date +%s%3N)

# 等待连接建立
sleep 2

# 停止抓包
echo "$PASSWORD" | sudo -S kill $tcpdump_pid 2>/dev/null
wait $tcpdump_pid 2>/dev/null

# 计算时延
baseline_latency=$((end_time - start_time))

# 检查连接状态
if echo "$PASSWORD" | sudo -S swanctl --list-sas --ike baseline 2>/dev/null | grep -q "ESTABLISHED"; then
    echo -e "${GREEN}✓ 基线连接建立成功${NC}"
    echo "  握手时延: ${baseline_latency} ms"

    # 获取报文统计
    packet_count=$(echo "$PASSWORD" | sudo -S tcpdump -r "$RESULTS_DIR/baseline_$TIMESTAMP.pcap" 2>/dev/null | wc -l)
    file_size=$(echo "$PASSWORD" | sudo -S ls -lh "$RESULTS_DIR/baseline_$TIMESTAMP.pcap" | awk '{print $5}')

    echo "  捕获报文数: $packet_count"
    echo "  抓包文件大小: $file_size"

    # 终止连接
    echo "$PASSWORD" | sudo -S swanctl --terminate --ike baseline >/dev/null 2>&1
else
    echo -e "${RED}✗ 基线连接失败${NC}"
    echo "  查看日志: $RESULTS_DIR/baseline_log_$TIMESTAMP.txt"
fi
echo ""

# 5. 测试混合密钥交换连接
echo "=========================================="
echo "5. 测试混合密钥交换 (x25519 + ML-KEM-768)"
echo "=========================================="
echo ""

# 启动抓包
echo "$PASSWORD" | sudo -S tcpdump -i ens33 -w "$RESULTS_DIR/hybrid_$TIMESTAMP.pcap" "port 500 or port 4500" 2>/dev/null &
tcpdump_pid=$!
sleep 1

# 发起连接
echo "发起连接..."
start_time=$(date +%s%3N)
echo "$PASSWORD" | sudo -S swanctl --initiate --ike pqgm-hybrid 2>&1 | tee "$RESULTS_DIR/hybrid_log_$TIMESTAMP.txt"
end_time=$(date +%s%3N)

# 等待连接建立
sleep 2

# 停止抓包
echo "$PASSWORD" | sudo -S kill $tcpdump_pid 2>/dev/null
wait $tcpdump_pid 2>/dev/null

# 计算时延
hybrid_latency=$((end_time - start_time))

# 检查连接状态
if echo "$PASSWORD" | sudo -S swanctl --list-sas --ike pqgm-hybrid 2>/dev/null | grep -q "ESTABLISHED"; then
    echo -e "${GREEN}✓ 混合连接建立成功${NC}"
    echo "  握手时延: ${hybrid_latency} ms"

    # 获取报文统计
    packet_count=$(echo "$PASSWORD" | sudo -S tcpdump -r "$RESULTS_DIR/hybrid_$TIMESTAMP.pcap" 2>/dev/null | wc -l)
    file_size=$(echo "$PASSWORD" | sudo -S ls -lh "$RESULTS_DIR/hybrid_$TIMESTAMP.pcap" | awk '{print $5}')

    echo "  捕获报文数: $packet_count"
    echo "  抓包文件大小: $file_size"

    # 终止连接
    echo "$PASSWORD" | sudo -S swanctl --terminate --ike pqgm-hybrid >/dev/null 2>&1
else
    echo -e "${RED}✗ 混合连接失败${NC}"
    echo "  查看日志: $RESULTS_DIR/hybrid_log_$TIMESTAMP.txt"
fi
echo ""

# 6. 生成测试报告
echo "=========================================="
echo "6. 测试报告"
echo "=========================================="
echo ""

cat > "$RESULTS_DIR/report_$TIMESTAMP.txt" <<EOF
PQGM-IKEv2 测试报告
生成时间: $(date)
==========================================

环境信息:
- Initiator: $(ip addr show ens33 | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1)
- Responder: $RESPONDER_IP
- strongSwan: $(swanctl --version 2>&1 | head -1)

连接测试结果:
------------------------------------------

1. 传统 IKEv2 (baseline: x25519)
   状态: $(echo "$PASSWORD" | sudo -S swanctl --list-sas --ike baseline 2>/dev/null | grep -q "ESTABLISHED" && echo "成功" || echo "失败")
   握手时延: ${baseline_latency} ms
   抓包文件: baseline_$TIMESTAMP.pcap

2. 混合密钥交换 (pqgm-hybrid: x25519 + ML-KEM-768)
   状态: $(echo "$PASSWORD" | sudo -S swanctl --list-sas --ike pqgm-hybrid 2>/dev/null | grep -q "ESTABLISHED" && echo "成功" || echo "失败")
   握手时延: ${hybrid_latency} ms
   抓包文件: hybrid_$TIMESTAMP.pcap

通信开销对比:
------------------------------------------
EOF

# 添加报文统计
echo "" >> "$RESULTS_DIR/report_$TIMESTAMP.txt"
echo "基线连接报文统计:" >> "$RESULTS_DIR/report_$TIMESTAMP.txt"
echo "$PASSWORD" | sudo -S tcpdump -r "$RESULTS_DIR/baseline_$TIMESTAMP.pcap" -q 2>/dev/null >> "$RESULTS_DIR/report_$TIMESTAMP.txt"
echo "" >> "$RESULTS_DIR/report_$TIMESTAMP.txt"
echo "混合连接报文统计:" >> "$RESULTS_DIR/report_$TIMESTAMP.txt"
echo "$PASSWORD" | sudo -S tcpdump -r "$RESULTS_DIR/hybrid_$TIMESTAMP.pcap" -q 2>/dev/null >> "$RESULTS_DIR/report_$TIMESTAMP.txt"

# 显示报告
cat "$RESULTS_DIR/report_$TIMESTAMP.txt"
echo ""

echo "=========================================="
echo "  测试完成"
echo "=========================================="
echo ""
echo "结果文件保存在: $RESULTS_DIR"
echo ""
echo "查看详细报告:"
echo "  cat $RESULTS_DIR/report_$TIMESTAMP.txt"
echo ""
echo "使用 Wireshark 分析抓包:"
echo "  wireshark $RESULTS_DIR/baseline_$TIMESTAMP.pcap"
echo "  wireshark $RESULTS_DIR/hybrid_$TIMESTAMP.pcap"
