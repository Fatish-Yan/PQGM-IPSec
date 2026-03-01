#!/bin/bash
# Docker双端测试脚本

set -e

SCRIPT_DIR=$(dirname "$0")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")

echo "=== PQ-GM-IKEv2 Docker 双端测试 ==="
echo ""

# 检查是否以root运行
if [ "$EUID" -ne 0 ]; then
    echo "请使用 sudo 运行此脚本"
    exit 1
fi

cd "$PROJECT_DIR/docker"

echo "1. 停止旧容器..."
docker-compose down 2>/dev/null || true

echo "2. 启动新容器..."
docker-compose up -d

echo "3. 等待容器启动..."
sleep 5

echo "4. 运行ldconfig..."
docker exec pqgm-initiator ldconfig
docker exec pqgm-responder ldconfig

echo "5. 启动charon..."
docker exec -d pqgm-responder /usr/local/libexec/ipsec/charon
sleep 2
docker exec -d pqgm-initiator /usr/local/libexec/ipsec/charon

echo "6. 等待charon就绪..."
sleep 3

echo "7. 加载配置..."
echo "   Responder:"
docker exec pqgm-responder /usr/local/sbin/swanctl --load-all 2>&1 | tail -3
echo "   Initiator:"
docker exec pqgm-initiator /usr/local/sbin/swanctl --load-all 2>&1 | tail -3

echo "8. 发起连接..."
docker exec pqgm-initiator /usr/local/sbin/swanctl --initiate --child ipsec 2>&1 | head -50

echo ""
echo "=== 测试完成 ==="
echo ""
echo "查看完整日志:"
echo "  docker exec pqgm-initiator cat /var/log/syslog"
echo "  docker exec pqgm-responder cat /var/log/syslog"
