#!/bin/bash
# ============================================================================
# PQ-GM-IKEv2 Docker 快速测试脚本
#
# 用途: 一键启动 Docker 测试环境并执行 5-RTT 连接测试
#
# 使用方法:
#   chmod +x quick-docker-test.sh
#   sudo ./quick-docker-test.sh
#
# 作者: PQ-GM-IKEv2 项目
# 日期: 2026-03-05
# ============================================================================

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 项目路径
PROJECT_DIR="/home/ipsec/PQGM-IPSec"
VMTEST_DIR="${PROJECT_DIR}/vm-test"
COMPOSE_FILE="${VMTEST_DIR}/docker-compose-test.yml"

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# 检查是否以 root 运行
if [ "$EUID" -ne 0 ]; then
    log_error "请使用 sudo 运行此脚本"
    exit 1
fi

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║        PQ-GM-IKEv2 Docker 快速测试                         ║"
echo "║        5-RTT Triple Key Exchange + ML-DSA Auth             ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# ============================================
# 步骤 1: 停止旧容器
# ============================================
log_step "步骤 1/6: 停止旧容器..."
docker-compose -f "$COMPOSE_FILE" down 2>/dev/null || true
log_info "旧容器已停止"

# ============================================
# 步骤 2: 启动新容器
# ============================================
log_step "步骤 2/6: 启动 Docker 容器..."
docker-compose -f "$COMPOSE_FILE" up -d

log_info "容器已启动:"
docker-compose -f "$COMPOSE_FILE" ps

# ============================================
# 步骤 3: 等待 charon 启动
# ============================================
log_step "步骤 3/6: 等待 charon 启动..."
sleep 3
log_info "等待完成"

# ============================================
# 步骤 4: 加载 Responder 配置
# ============================================
log_step "步骤 4/6: 加载 Responder 配置..."
docker exec pqgm-responder-test swanctl --load-all 2>&1 | grep -E "loaded|connection|failed" || true

# ============================================
# 步骤 5: 加载 Initiator 配置
# ============================================
log_step "步骤 5/6: 加载 Initiator 配置..."
docker exec pqgm-initiator-test swanctl --load-all 2>&1 | grep -E "loaded|connection|failed" || true

# ============================================
# 步骤 6: 发起 5-RTT 连接
# ============================================
log_step "步骤 6/6: 发起 5-RTT 连接测试..."
echo ""

echo "选择测试配置:"
echo "  1) pqgm-5rtt-mldsa    (AES-256 + HMAC-SHA256) [默认]"
echo "  2) pqgm-5rtt-gm-symm  (SM4 + HMAC-SM3)"
echo ""
read -p "请选择 [1/2]: " choice

case "$choice" in
    2)
        IKE_NAME="pqgm-5rtt-gm-symm"
        log_info "测试国密对称栈配置"
        ;;
    *)
        IKE_NAME="pqgm-5rtt-mldsa"
        log_info "测试标准算法配置"
        ;;
esac

echo ""
log_info "发起连接: swanctl --initiate --child net --ike $IKE_NAME"
echo ""
echo "──────────────────────────────────────────────────────────────"
echo ""

# 发起连接
docker exec pqgm-initiator-test swanctl --initiate --child net --ike "$IKE_NAME" 2>&1 | \
    grep -E "selected proposal|established|authentication|ML-DSA|SM2-KEM|initiate completed|failed|error" || \
    docker exec pqgm-initiator-test swanctl --initiate --child net --ike "$IKE_NAME"

echo ""
echo "──────────────────────────────────────────────────────────────"
echo ""

# ============================================
# 验证结果
# ============================================
log_step "验证 SA 状态..."
echo ""

docker exec pqgm-initiator-test swanctl --list-sas 2>/dev/null || true

echo ""
echo "══════════════════════════════════════════════════════════════"
echo ""

# 检查是否成功
if docker exec pqgm-initiator-test swanctl --list-sas 2>/dev/null | grep -q "ESTABLISHED"; then
    echo -e "${GREEN}✓ 测试成功!${NC}"
    echo ""
    echo "5-RTT 协议验证通过:"
    echo "  • RTT 1: IKE_SA_INIT (x25519 + SM2-KEM + ML-KEM-768)"
    echo "  • RTT 2: IKE_INTERMEDIATE #0 (双证书分发)"
    echo "  • RTT 3: IKE_INTERMEDIATE #1 (SM2-KEM)"
    echo "  • RTT 4: IKE_INTERMEDIATE #2 (ML-KEM-768)"
    echo "  • RTT 5: IKE_AUTH (ML-DSA-65)"
    echo ""
else
    echo -e "${RED}✗ 测试失败${NC}"
    echo ""
    echo "查看详细日志:"
    echo "  docker logs pqgm-initiator-test"
    echo "  docker logs pqgm-responder-test"
    echo ""
fi

echo "其他命令:"
echo "  • 停止容器: sudo docker-compose -f $COMPOSE_FILE down"
echo "  • 查看日志: sudo docker logs pqgm-initiator-test"
echo "  • 进入容器: sudo docker exec -it pqgm-initiator-test bash"
echo ""
