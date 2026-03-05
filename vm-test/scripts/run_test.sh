#!/bin/bash
# PQ-GM-IKEv2 性能测试脚本
# 用法: ./run_test.sh <配置名> [测试轮次]
# 示例: ./run_test.sh pqgm-5rtt-mldsa 10

set -e

CONFIG=${1:-pqgm-5rtt-mldsa}
ROUNDS=${2:-10}
OUTPUT_DIR=/home/ipsec/PQGM-IPSec/vm-test/results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="${OUTPUT_DIR}/test_${CONFIG}_${TIMESTAMP}.csv"

echo "========================================"
echo "PQ-GM-IKEv2 性能测试"
echo "========================================"
echo "配置: $CONFIG"
echo "轮次: $ROUNDS"
echo "输出: $OUTPUT"
echo "========================================"

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# CSV表头
echo "round,start_time,end_time,duration_ms,status,ike_sa,child_sa" > "$OUTPUT"

# 检查strongSwan状态
if ! pgrep -x charon > /dev/null; then
    echo "[!] charon未运行，正在启动..."
    sudo systemctl start strongswan
    sleep 2
fi

# 预热轮次 (不计入统计)
echo ""
echo "[*] 预热阶段 (3轮)..."
for i in 1 2 3; do
    echo -n "  预热 $i... "
    swanctl --terminate --ike all 2>/dev/null || true
    sleep 2
    RESULT=$(swanctl --initiate --child net --ike "$CONFIG" 2>&1)
    if echo "$RESULT" | grep -q "established"; then
        echo "OK"
    else
        echo "FAILED"
    fi
    sleep 3
done

# 清除预热SA
swanctl --terminate --ike all 2>/dev/null || true
sleep 2

# 正式测试
echo ""
echo "[*] 正式测试开始..."
for i in $(seq 1 "$ROUNDS"); do
    echo -n "  Round $i/$ROUNDS: "

    # 清除旧SA
    swanctl --terminate --ike all 2>/dev/null || true
    sleep 2

    # 记录开始时间
    START=$(date +%s%3N)

    # 发起连接
    RESULT=$(swanctl --initiate --child net --ike "$CONFIG" 2>&1)

    # 记录结束时间
    END=$(date +%s%3N)
    DURATION=$((END - START))

    # 解析结果
    if echo "$RESULT" | grep -q "established"; then
        STATUS="success"
        IKE_SA=$(echo "$RESULT" | grep -oP 'IKE_SA: \K[^,]+' || echo "unknown")
        CHILD_SA=$(echo "$RESULT" | grep -oP 'CHILD_SA: \K[^,]+' || echo "unknown")
        echo -e "\033[32m$DURATION ms\033[0m (IKE: $IKE_SA)"
    else
        STATUS="failed"
        IKE_SA=""
        CHILD_SA=""
        echo -e "\033[31mFAILED\033[0m"
        # 打印错误信息
        echo "$RESULT" | grep -i "error\|failed\|unacceptable" | head -3 | sed 's/^/      /'
    fi

    # 写入CSV
    echo "$i,$START,$END,$DURATION,$STATUS,\"$IKE_SA\",\"$CHILD_SA\"" >> "$OUTPUT"

    # 间隔
    sleep 5
done

# 统计结果
echo ""
echo "========================================"
echo "测试完成!"
echo "========================================"

# 计算统计信息
SUCCESS_COUNT=$(grep -c ",success," "$OUTPUT" || echo 0)
FAIL_COUNT=$(grep -c ",failed," "$OUTPUT" || echo 0)

if [ "$SUCCESS_COUNT" -gt 0 ]; then
    # 计算成功轮次的平均延迟
    AVG=$(awk -F',' '$5=="success" {sum+=$4; count++} END {if(count>0) printf "%.1f", sum/count}' "$OUTPUT")
    MIN=$(awk -F',' '$5=="success" {print $4}' "$OUTPUT" | sort -n | head -1)
    MAX=$(awk -F',' '$5=="success" {print $4}' "$OUTPUT" | sort -n | tail -1)

    echo "成功: $SUCCESS_COUNT / $ROUNDS"
    echo "平均延迟: ${AVG} ms"
    echo "最小延迟: ${MIN} ms"
    echo "最大延迟: ${MAX} ms"
else
    echo "所有测试失败!"
fi

echo ""
echo "详细结果: $OUTPUT"
echo "========================================"
