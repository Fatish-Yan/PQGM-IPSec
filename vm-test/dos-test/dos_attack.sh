#!/bin/bash
# DoS 攻击模拟脚本 - 改进版 v2
# 用于测试早期门控机制的效果
# 策略：获取 SA ID 后逐个清理

set -e

# 配置参数
FREQUENCY="${FREQUENCY:-50}"        # 每秒请求数
DURATION="${DURATION:-60}"          # 持续时间（秒）
IKE_NAME="${IKE_NAME:-pqgm-5rtt-mldsa}"  # 连接名称
CLEANUP_INTERVAL="${CLEANUP_INTERVAL:-100}"  # 每多少请求清理一次 SA

# 计算总请求数
TOTAL_REQUESTS=$((FREQUENCY * DURATION))
DELAY=$(echo "scale=4; 1/$FREQUENCY" | bc)

echo "========================================"
echo "DoS 攻击模拟测试 (改进版 v2)"
echo "========================================"
echo "频率: $FREQUENCY 次/秒"
echo "持续时间: $DURATION 秒"
echo "总请求数: $TOTAL_REQUESTS"
echo "间隔: ${DELAY} 秒"
echo "连接名: $IKE_NAME (5-RTT PQGM)"
echo "清理间隔: 每 $CLEANUP_INTERVAL 请求"
echo "开始时间: $(date)"
echo "========================================"

# 清理所有 IKE SA
cleanup_sas() {
    # 获取所有 SA ID 并逐个终止
    local sa_ids=$(echo "1574a" | sudo -S swanctl --list-sas 2>/dev/null | grep -oP '#\K[0-9]+' || true)
    for sa_id in $sa_ids; do
        echo "1574a" | sudo -S swanctl --terminate --ike-id "$sa_id" >/dev/null 2>&1 || true
    done
}

# 初始清理
cleanup_sas

# 记录开始时间
START_TIME=$(date +%s)
REQUEST_SENT=0

# 攻击循环
for i in $(seq 1 $TOTAL_REQUESTS); do
    # 后台发起连接请求
    echo "1574a" | sudo -S swanctl --initiate --ike "$IKE_NAME" >/dev/null 2>&1 &
    REQUEST_SENT=$((REQUEST_SENT + 1))

    # 显示进度
    if (( i % 50 == 0 )); then
        ELAPSED=$(($(date +%s) - START_TIME))
        # 统计当前 SA 数量
        SA_COUNT=$(echo "1574a" | sudo -S swanctl --list-sas 2>/dev/null | grep -c "ESTABLISHED" || echo "0")
        echo "已发送: $i/$TOTAL_REQUESTS 请求, 已用时: ${ELAPSED}秒, 当前 SA: $SA_COUNT"
    fi

    # 定期清理 SA，防止连接池满
    if (( i % CLEANUP_INTERVAL == 0 )); then
        cleanup_sas
    fi

    # 控制频率
    sleep "$DELAY"
done

# 等待所有后台任务完成
wait

# 最终清理
cleanup_sas

# 记录结束时间
END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

echo "========================================"
echo "攻击模拟完成"
echo "结束时间: $(date)"
echo "实际耗时: ${TOTAL_TIME}秒"
echo "发送请求: $REQUEST_SENT"
echo "========================================"