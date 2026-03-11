#!/bin/bash
# DoS 门控机制对比测试脚本
# 测试有无门控机制下的连接成功率和响应时间

set -e

# 测试参数
ATTACK_RATE="${ATTACK_RATE:-100}"      # 攻击请求/秒
ATTACK_DURATION="${ATTACK_DURATION:-10}" # 攻击持续时间(秒)
LEGITIMATE_RATE="${LEGITIMATE_RATE:-1}"  # 合法请求/秒
TEST_DURATION="${TEST_DURATION:-30}"    # 总测试时长(秒)
OUTPUT_DIR="/home/ipsec/PQGM-IPSec/vm-test/results"

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "========================================"
echo "DoS 门控机制对比测试"
echo "========================================"
echo "攻击速率: $ATTACK_RATE 请求/秒"
echo "攻击时长: $ATTACK_DURATION 秒"
echo "合法请求速率: $LEGITIMATE_RATE 请求/秒"
echo "测试时长: $TEST_DURATION 秒"
echo "========================================"

# 函数：运行单次测试
run_test() {
    local gating_mode=$1
    local test_name=$2
    local output_file="$OUTPUT_DIR/${test_name}_$(date +%Y%m%d_%H%M%S).log"

    echo ""
    echo -e "${YELLOW}=== 测试: $test_name (门控模式: $gating_mode) ===${NC}"

    # 重启容器设置门控模式
    echo "重启容器并设置 DOS_GATING_MODE=$gating_mode..."
    cd /home/ipsec/PQGM-IPSec/vm-test

    # 停止容器
    sudo docker-compose -f docker-compose-test.yml down 2>/dev/null || true
    sleep 2

    # 修改环境变量并启动
    export DOS_GATING_MODE=$gating_mode
    sudo -E docker-compose -f docker-compose-test.yml up -d
    sleep 5

    # 验证门控模式
    ACTUAL_MODE=$(sudo docker exec pqgm-responder-test bash -c 'echo $DOS_GATING_MODE' 2>/dev/null || echo "unknown")
    echo "实际门控模式: $ACTUAL_MODE"

    # 初始化 strongSwan
    echo "初始化 strongSwan..."
    sudo docker exec pqgm-initiator-test swanctl --load-all >/dev/null 2>&1 || true
    sudo docker exec pqgm-responder-test swanctl --load-all >/dev/null 2>&1 || true
    sleep 2

    # 开始攻击（后台）
    echo "启动 DoS 攻击模拟..."
    sudo docker exec pqgm-initiator-test bash -c "
        for i in \$(seq 1 $((ATTACK_RATE * ATTACK_DURATION))); do
            swanctl --initiate --ike pqgm-5rtt-mldsa --child child_sa >/dev/null 2>&1 &
            usleep $((1000000 / ATTACK_RATE))
        done
    " &
    ATTACK_PID=$!

    # 监控合法连接
    echo "监控合法连接..."
    local success_count=0
    local fail_count=0
    local total_time=0
    local start_time=$(date +%s)

    while [ $(($(date +%s) - start_time)) -lt $TEST_DURATION ]; do
        # 每秒发起一次合法连接
        local conn_start=$(date +%s%N)
        if sudo docker exec pqgm-initiator-test timeout 10 swanctl --initiate --ike pqgm-5rtt-mldsa --child child_sa 2>&1 | grep -q "ESTABLISHED"; then
            local conn_end=$(date +%s%N)
            local conn_time=$(( (conn_end - conn_start) / 1000000 ))
            success_count=$((success_count + 1))
            total_time=$((total_time + conn_time))
            echo -e "  ${GREEN}✓${NC} 连接成功 (${conn_time}ms)"
        else
            fail_count=$((fail_count + 1))
            echo -e "  ${RED}✗${NC} 连接失败"
        fi

        sleep $((1 / LEGITIMATE_RATE))
    done

    # 等待攻击完成
    wait $ATTACK_PID 2>/dev/null || true

    # 计算结果
    local success_rate=0
    local avg_time=0
    if [ $((success_count + fail_count)) -gt 0 ]; then
        success_rate=$(echo "scale=2; $success_count * 100 / ($success_count + $fail_count)" | bc)
    fi
    if [ $success_count -gt 0 ]; then
        avg_time=$((total_time / success_count))
    fi

    # 输出结果
    echo ""
    echo "----------------------------------------"
    echo "测试结果: $test_name"
    echo "----------------------------------------"
    echo "成功连接: $success_count"
    echo "失败连接: $fail_count"
    echo "成功率: $success_rate%"
    echo "平均响应时间: ${avg_time}ms"
    echo "----------------------------------------"

    # 保存到文件
    echo "门控模式: $gating_mode" > "$output_file"
    echo "测试时间: $(date)" >> "$output_file"
    echo "攻击速率: $ATTACK_RATE 请求/秒" >> "$output_file"
    echo "成功连接: $success_count" >> "$output_file"
    echo "失败连接: $fail_count" >> "$output_file"
    echo "成功率: $success_rate%" >> "$output_file"
    echo "平均响应时间: ${avg_time}ms" >> "$output_file"

    echo "结果已保存到: $output_file"
}

# 主测试流程
echo ""
echo -e "${YELLOW}开始对比测试...${NC}"

# 测试1: 无门控
run_test "none" "no_gating"

# 测试2: 有门控
run_test "block" "with_gating"

echo ""
echo "========================================"
echo "对比测试完成！"
echo "结果保存在: $OUTPUT_DIR"
echo "========================================"