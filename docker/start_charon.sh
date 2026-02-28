#!/bin/bash
# 启动 charon 的脚本

export LD_PRELOAD=/usr/local/lib/libgmssl.so.3

# 创建配置目录链接
mkdir -p /usr/local/etc/swanctl
cp /etc/swanctl/* /usr/local/etc/swanctl/ -r 2>/dev/null || true
cp /etc/strongswan.conf /usr/local/etc/ 2>/dev/null || true

# 启动 charon
exec /usr/local/libexec/ipsec/charon --debug-ike 2
