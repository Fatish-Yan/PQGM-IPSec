#!/bin/bash
# 握手时延测试脚本 - 测量 IKEv2 握手时间

PASSWORD="1574a"
ITERATIONS=1000

echo "=========================================="
echo "  PQGM-IKEv2 握手时延测试"
echo "=========================================="
echo ""

# 函数：测试连接并测量时延
test_connection() {
    local conn_name=$1
    local iterations=$2

    echo "测试连接: $conn_name"
    echo "迭代次数: $iterations"
    echo "------------------------"

    # 清理现有连接
    echo '1574a' | sudo -S swanctl --terminate --ike "$conn_name" >/dev/null 2>&1

    total_time=0
    success_count=0

    for i in $(seq 1 $iterations); do
        # 记录开始时间
        start_time=$(date +%s%N)

        # 启动连接
        echo '1574a' | sudo -S swanctl --initiate --child "$conn_name" >/dev/null 2>&1

        # 等待连接建立
        sleep 0.1

        # 记录结束时间
        end_time=$(date +%s%N)

        # 计算耗时（毫秒）
        elapsed=$((($end_time - $start_time) / 1000000))

        # 检查连接状态
        if echo '1574a' | sudo -S swanctl --list-sas --ike "$conn_name" 2>/dev/null | grep -q "ESTABLISHED"; then
            total_time=$(($total_time + $elapsed))
            success_count=$(($success_count + 1))

            # 终止连接以进行下一次测试
            echo '1574a' | sudo -S swanctl --terminate --ike "$conn_name" >/dev/null 2>&1
        fi

        # 每100次显示进度
        if [ $((i % 100)) -eq 0 ]; then
            echo "进度: $i / $iterations"
        fi
    done

    # 计算平均时延
    if [ $success_count -gt 0 ]; then
        avg_time=$(($total_time / $success_count))
        echo ""
        echo "成功连接: $success_count / $iterations"
        echo "平均握手时延: ${avg_time} ms"
        echo "------------------------"
        echo ""
    else
        echo "错误: 没有成功建立任何连接！"
        echo "请检查："
        echo "  1. Responder (192.168.172.131) 是否运行"
        echo "  2. 网络连接是否正常"
        echo "  3. 证书配置是否正确"
        echo ""
    fi
}

# 测试基线连接（传统 IKEv2）
echo "1. 传统 IKEv2 基线测试 (x25519)"
echo ""
test_connection "baseline" $ITERATIONS

# 测试混合密钥交换连接
echo "2. 混合密钥交换测试 (x25519 + ML-KEM-768)"
echo ""
test_connection "pqgm-hybrid" $ITERATIONS

echo "=========================================="
echo "  测试完成"
echo "=========================================="
