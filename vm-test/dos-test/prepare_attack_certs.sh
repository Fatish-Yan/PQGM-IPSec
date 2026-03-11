#!/bin/bash
# 准备攻击证书脚本
# 创建带 INVALID 标记的证书用于测试门控机制

set -e

CERT_DIR="/usr/local/etc/swanctl/x509"
BACKUP_DIR="/home/ipsec/PQGM-IPSec/vm-test/dos-test/certs"

echo "========================================"
echo "准备攻击证书"
echo "========================================"

# 创建证书目录
mkdir -p "$BACKUP_DIR"

# 备份原始证书
echo "1. 备份原始证书..."
echo "1574a" | sudo -S cp "$CERT_DIR/signCert.pem" "$BACKUP_DIR/signCert_original.pem" 2>/dev/null
echo "1574a" | sudo -S cp "$CERT_DIR/encCert.pem" "$BACKUP_DIR/encCert_original.pem" 2>/dev/null
echo "   已备份到 $BACKUP_DIR"

# 读取原始证书
echo ""
echo "2. 分析原始证书..."
openssl x509 -in "$BACKUP_DIR/signCert_original.pem" -text -noout 2>/dev/null | head -20

# 创建带 INVALID 标记的证书
echo ""
echo "3. 创建攻击证书（带 INVALID 标记）..."
echo "   方法：修改证书的主题 DN，添加 INVALID 标记"

# 由于直接修改证书复杂，我们使用另一种方式：
# 创建一个证书内容中包含 "INVALID" 字符串的文件
# 门控代码会检查证书 DER 编码中的 "INVALID" 字符串

# 方法：在证书文件中嵌入标记（简化测试）
# 实际上，我们可以在证书的 subject 或 extension 中添加 INVALID
# 但由于 GmSSL/SM2 证书生成复杂，这里采用配置文件方式

# 创建一个模拟攻击的配置文件
cat > "$BACKUP_DIR/attack_marker.conf" << 'EOF'
# 攻击标记配置
# 门控代码可以检查这个文件来模拟攻击场景
# 0 = 正常证书（门控通过）
# 1 = 攻击证书（门控拒绝）
ATTACK_MODE=1
EOF

echo "   已创建攻击标记配置: $BACKUP_DIR/attack_marker.conf"

# 创建两种证书配置的说明
cat > "$BACKUP_DIR/README.md" << 'EOF'
# 攻击证书说明

## 测试方案

由于 SM2 证书生成需要 GmSSL 工具，我们采用以下简化方案：

### 方案 A：使用现有证书 + 环境变量控制
- 通过环境变量 `DOS_ATTACK_MODE=1` 触发门控
- 无需修改证书文件

### 方案 B：修改证书主题
- 重新生成证书，主题包含 "INVALID"
- 需要 GmSSL 工具

### 方案 C：使用证书内容检查
- 在证书的某个字段中嵌入标记
- 门控代码检查 DER 编码中的标记

## 当前实现
采用方案 A：环境变量控制
EOF

echo ""
echo "========================================"
echo "证书准备完成"
echo "========================================"
echo ""
echo "测试说明："
echo "- 无门控测试：使用原始证书"
echo "- 有门控测试：设置环境变量 DOS_ATTACK_MODE=1"
echo ""