#!/bin/bash
# 集成 gmalg 插件到 strongSwan 配置

set -e

STRONGSWAN_DIR="/home/ipsec/strongswan"
GMALG_DIR="$STRONGSWAN_DIR/src/libstrongswan/plugins/gmalg"

echo "=========================================="
echo "集成 gmalg 插件到 strongSwan"
echo "=========================================="
echo ""

# 1. 在 configure.ac 中添加 gmalg 插件配置
echo "1. 修改 configure.ac..."

# 在 ARG_ENABL_SET 部分添加 gmalg 配置（大约在第 160 行之后）
# 查找 sha1 配置行号
SHA1_LINE=$(grep -n "ARG_ENABL_SET(\[sha1\]" $STRONGSWAN_DIR/configure.ac | cut -d: -f1)

# 在 sha1 配置后添加 gmalg 配置
sed -i "${SHA1_LINE}a\\
ARG_ENABL_SET([gmalg],               [enable Chinese National Crypto Algorithms (SM2/SM3/SM4) plugin.])\\
" $STRONGSWAN_DIR/configure.ac

# 2. 在插件检测部分添加 gmalg 检测（大约在第 462 行附近）
SHA1_DETECT_LINE=$(grep -n "sha1=true;" $STRONGSWAN_DIR/configure.ac | head -1 | cut -d: -f1)
sed -i "${SHA1_DETECT_LINE}a\\
	if test x\$enable_gmalg = xyes; then\\
		gmalg=true\\
		AM_CONDITIONAL(USE_GMALG, true)\\
		echo \"HAVE_GMSSL\" >>config.h\\
	else\\
		gmalg=false\\
		AM_CONDITIONAL(USE_GMALG, false)\\
	fi\\
" $STRONGSWAN_DIR/configure.ac

# 3. 在 ADD_PLUGIN 部分添加 gmalg 插件（在 sha1 之后）
SHA1_ADD_LINE=$(grep -n "ADD_PLUGIN(\[sha1\]" $STRONGSWAN_DIR/configure.ac | head -1 | cut -d: -f1)
sed -i "${SHA1_ADD_LINE}a\\
ADD_PLUGIN([gmalg],               [s charon swanctl pki scripts nm cmd])\\
" $STRONGSWAN_DIR/configure.ac

# 4. 在 AM_CONDITIONAL 部分添加（在 USE_SHA1 之后）
SHA1_COND_LINE=$(grep -n "AM_CONDITIONAL(USE_SHA1" $STRONGSWAN_DIR/configure.ac | head -1 | cut -d: -f1)
sed -i "${SHA1_COND_LINE}a\\
AM_CONDITIONAL(USE_GMALG, test x\$gmalg = xtrue)\\
" $STRONGSWAN_DIR/configure.ac

# 5. 在 Makefile 列表部分添加 gmalg
FIND_MAKEFILE_LINE=$(grep -n "src/libstrongswan/plugins/sha1/Makefile" $STRONGSWAN_DIR/configure.ac | head -1 | cut -d: -f1)
sed -i "${FIND_MAKEFILE_LINE}a\\
	src/libstrongswan/plugins/gmalg/Makefile\\
" $STRONGSWAN_DIR/configure.ac

echo "  configure.ac 修改完成"
echo ""

# 6. 创建 plugins/Makefile.am（如果不存在）
PLUGINS_MAKEFILE="$STRONGSWAN_DIR/src/libstrongswan/plugins/Makefile.am"
if [ ! -f "$PLUGINS_MAKEFILE" ]; then
    echo "2. 创建 plugins/Makefile.am..."
    cat > "$PLUGINS_MAKEFILE" << 'EOF'
# strongSwan library plugins

if USE_GMALG
GMALG_SUBDIR = gmalg
endif

SUBDIRS = \
	aes \
	$(GMALG_SUBDIR) \
	sha1 \
	sha2 \
	hmac \
	x509 \
	$(NULL)
EOF
    echo "  plugins/Makefile.am 创建完成"
else
    echo "2. plugins/Makefile.am 已存在，添加 gmalg..."
    # 在 SUBDIRS 中添加 gmalg
    sed -i 's/SUBDIRS = ./SUBDIRS = \\\n\tgmalg \\/' "$PLUGINS_MAKEFILE"
fi
echo ""

echo "=========================================="
echo "配置修改完成！"
echo "=========================================="
echo ""
echo "下一步操作:"
echo "1. cd /home/ipsec/strongswan"
echo "2. autoreconf --force --install"
echo "3. ./configure --enable-gmalg"
echo "4. make"
echo ""
