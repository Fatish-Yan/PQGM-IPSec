#!/bin/bash
# DoS 门控机制对比测试脚本
# 测试有无门控机制下的连接成功率

set -e

# 测试参数
ATTACK_RATE="${ATTACK_RATE:-50}"      # 攻击请求/秒
TEST_DURATION="${TEST_DURATION:-10}"  # 测试时长(秒)
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
    echo "重启容器..."
    cd /home/ipsec/PQGM-IPSec/vm-test
    echo "1574a" | sudo -S docker-compose -f docker-compose-test.yml down 2>/dev/null || true
    sleep 2

    # 修改环境变量
    sed -i "s/DOS_GATING_MODE=.*/DOS_GATING_MODE=$gating_mode/" docker-compose-test.yml

    echo "1574a" | sudo -S docker-compose -f docker-compose-test.yml up -d
    sleep 5

    # 验证门控模式
    ACTUAL_MODE=$(echo "1574a" | sudo docker exec pqgm-responder-test bash -c 'echo $DOS_GATING_MODE' 2>/dev/null || echo "unknown")
    echo "实际门控模式: $ACTUAL_MODE"

    # 初始化 strongSwan
    echo "初始化 strongSwan..."
    echo "1574a" | sudo docker exec pqgm-initiator-test swanctl --load-all >/dev/null 2>&1 || true
    echo "1574a" | sudo docker exec pqgm-responder-test swanctl --load-all >/dev/null 2>&1 || true
    sleep 2

    # 开始攻击（后台）
    echo "启动 DoS 攻击模拟 ($ATTACK_RATE 请求/秒)..."
    echo "1574a" | sudo docker exec pqgm-initiator-test bash -c "
        for i in \$(seq 1 $((ATTACK_RATE * TEST_DURATION))); do
            swanctl --initiate --ike pqgm-5rtt-mldsa --child net >/dev/null 2>&1 &
            usleep $((1000000 / ATTACK_RATE))
        done
    " &
    ATTACK_PID=$!

    # 等待攻击开始
    sleep 1

    # 测试合法连接
    echo "测试合法连接..."
    local success_count=0
    local fail_count=0
    local total_time=0

    for i in 1 2 3 4 5; do
        local conn_start=$(date +%s%N)
        result=$(echo "1574a" | sudo docker exec pqgm-initiator-test timeout 15 swanctl --initiate --ike pqgm-5rtt-mldsa --child net 2>&1)
        local conn_end=$(date +%s%N)

        if echo "$result" | grep -q "ESTABLISHED\|established successfully"; then
            local conn_time=$(( (conn_end - conn_start) / 1000000 ))
            total_time=$((total_time + conn_time))
            success_count=$((success_count + 1))
            echo -e "  测试 $i: ${GREEN}✓ 成功${NC} (${conn_time}ms)"
        else
            fail_count=$((fail_count + 1))
            echo -e "  测试 $i: ${RED}✗ 失败${NC}"
        fi
        sleep 1
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
    echo "门控模式: $gating_mode"
    echo "攻击速率: $ATTACK_RATE 请求/秒"
    echo "成功连接: $success_count"
    echo "失败连接: $fail_count"
    echo "成功率: $success_rate%"
    echo "平均响应时间: ${avg_time}ms"
    echo "----------------------------------------"

    # 保存到文件
    {
        echo "测试时间: $(date)"
        echo "门控模式: $gating_mode"
        echo "攻击速率: $ATTACK_RATE 请求/秒"
        echo "成功连接: $success_count"
        echo "失败连接: $fail_count"
        echo "成功率: $success_rate%"
        echo "平均响应时间: ${avg_time}ms"
    } > "$output_file"

    echo "结果已保存到: $output_file"
}

# 主测试流程
echo ""
echo -e "${YELLOW}开始对比测试...${NC}"

# 测试1: 无门控
run_test "none" "no_gating"

# 测试2: 有门控
run_test "block" "with_gating"

# 恢复原始配置
sed -i "s/DOS_GATING_MODE=.*/DOS_GATING_MODE=block/" docker-compose-test.yml

echo ""
echo "========================================"
echo "对比测试完成！"
echo "结果保存在: $OUTPUT_DIR"
echo "========================================"

# 显示结果对比
echo ""
echo "=== 结果对比 ==="
ls -la "$OUTPUT_DIR"/*.log 2>/dev/null | tail -5
echo ""
for f in "$OUTPUT_DIR"/*.log; do
    if [ -f "$f" ]; then
        echo "--- $(basename $f) ---"
        cat "$f"
        echo ""
    fi
done | tail -30