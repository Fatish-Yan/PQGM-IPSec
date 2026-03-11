#!/bin/bash
# Docker 高频 DoS 攻击脚本
# 在容器内执行，测试门控机制效果

set -e

# 配置参数
FREQUENCY="${FREQUENCY:-500}"     # 每秒请求数（提高到 500）
DURATION="${DURATION:-30}"        # 持续时间（秒）
IKE_NAME="${IKE_NAME:-pqgm-5rtt-mldsa}"

# 计算总请求数
TOTAL_REQUESTS=$((FREQUENCY * DURATION))

echo "========================================"
echo "Docker DoS 攻击模拟测试"
echo "========================================"
echo "频率: $FREQUENCY 次/秒"
echo "持续时间: $DURATION 秒"
echo "总请求数: $TOTAL_REQUESTS"
echo "连接名: $IKE_NAME"
echo "开始时间: $(date)"
echo "========================================"

# 记录开始时间
START_TIME=$(date +%s)

# 攻击循环
for i in $(seq 1 $TOTAL_REQUESTS); do
    # 后台发起连接请求
    swanctl --initiate --ike "$IKE_NAME" >/dev/null 2>&1 &

    # 显示进度
    if (( i % 500 == 0 )); then
        ELAPSED=$(($(date +%s) - START_TIME))
        SA_COUNT=$(swanctl --list-sas 2>/dev/null | grep -c "ESTABLISHED" || echo "0")
        echo "已发送: $i/$TOTAL_REQUESTS 请求, 已用时: ${ELAPSED}秒, 当前 SA: $SA_COUNT"
    fi

    # 控制频率（微秒级）
    usleep $((1000000 / FREQUENCY))
done

# 等待所有后台任务完成
wait

# 记录结束时间
END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

echo "========================================"
echo "攻击模拟完成"
echo "结束时间: $(date)"
echo "实际耗时: ${TOTAL_TIME}秒"
echo "========================================"