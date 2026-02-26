#!/bin/bash
# 握手时延测试脚本 - 多次测试取平均值

PASSWORD="1574a"
RESULTS_DIR="/home/ipsec/pqgm-test/results"
TEST_COUNT=10

mkdir -p "$RESULTS_DIR"

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "=========================================="
echo "  握手时延测试 (每个配置测试 $TEST_COUNT 次)"
echo "=========================================="
echo ""

# 函数：测试单个配置
test_connection() {
    local conn_name=$1
    local conn_desc=$2

    echo "测试配置: $conn_desc ($conn_name)"
    echo "------------------------"

    local total_latency=0
    local success_count=0
    local latencies=()

    for i in $(seq 1 $TEST_COUNT); do
        # 确保没有现有连接
        echo "$PASSWORD" | sudo -S swanctl --terminate --ike $conn_name >/dev/null 2>&1
        sleep 1

        # 发起连接并计时
        start_time=$(date +%s%3N)
        output=$(echo "$PASSWORD" | sudo -S swanctl --initiate --ike $conn_name 2>&1)
        end_time=$(date +%s%3N)

        latency=$((end_time - start_time))

        # 检查是否成功
        if echo "$output" | grep -q "established"; then
            total_latency=$((total_latency + latency))
            latencies+=($latency)
            success_count=$((success_count + 1))
            echo -n "."
        else
            echo -n "x"
        fi

        # 终止连接
        sleep 1
        echo "$PASSWORD" | sudo -S swanctl --terminate --ike $conn_name >/dev/null 2>&1
        sleep 1
    done

    echo ""

    if [ $success_count -gt 0 ]; then
        avg_latency=$((total_latency / success_count))

        # 计算最小和最大值
        min_latency=${latencies[0]}
        max_latency=${latencies[0]}
        for lat in "${latencies[@]}"; do
            if [ $lat -lt $min_latency ]; then
                min_latency=$lat
            fi
            if [ $lat -gt $max_latency ]; then
                max_latency=$lat
            fi
        done

        echo -e "${GREEN}✓ 成功: $success_count/$TEST_COUNT${NC}"
        echo "  平均时延: ${avg_latency} ms"
        echo "  最小时延: ${min_latency} ms"
        echo "  最大时延: ${max_latency} ms"
        echo "  所有数据: ${latencies[@]}"

        # 保存结果
        echo "$conn_desc: ${latencies[@]}" > "$RESULTS_DIR/latency_${conn_name}.txt"
    else
        echo -e "${RED}✗ 所有测试失败${NC}"
    fi

    echo ""
}

# 测试基线连接
test_connection "baseline" "传统 IKEv2 (x25519)"

# 测试混合密钥交换
test_connection "pqgm-hybrid" "混合密钥交换 (x25519 + ML-KEM-768)"

echo "=========================================="
echo "  测试完成"
echo "=========================================="
echo ""
echo "结果文件保存在: $RESULTS_DIR"
echo ""

# 生成对比报告
echo "握手时延对比:"
echo "----------------------------------------"

if [ -f "$RESULTS_DIR/latency_baseline.txt" ]; then
    baseline_data=$(cat "$RESULTS_DIR/latency_baseline.txt" | cut -d: -f2)
    echo "传统 IKEv2 (x25519): $baseline_data"
fi

if [ -f "$RESULTS_DIR/latency_pqgm-hybrid.txt" ]; then
    hybrid_data=$(cat "$RESULTS_DIR/latency_pqgm-hybrid.txt" | cut -d: -f2)
    echo "混合密钥交换: $hybrid_data"
fi
