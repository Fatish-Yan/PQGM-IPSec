#!/bin/bash
# Responder 配置脚本 - 在克隆VM上运行

PASSWORD="1574a"
echo "=========================================="
echo "  PQGM-IKEv2 Responder 配置"
echo "=========================================="
echo ""

# 检查证书文件是否存在
if [ ! -f ~/pqgm-test/caCert.pem ]; then
    echo "错误: 证书文件不存在！"
    echo "请确保 ~/pqgm-test/ 目录包含所有必要的证书文件。"
    exit 1
fi

# 1. 配置网络
echo "1. 配置网络接口..."
echo '1574a' | sudo -S ip addr del 192.168.172.130/24 dev ens33 2>/dev/null
echo '1574a' | sudo -S ip addr add 192.168.172.131/24 dev ens33
echo "   IP 地址设置为: 192.168.172.131"
echo ""

# 2. 安装 Responder 证书
echo "2. 安装证书..."
echo '1574a' | sudo -S mkdir -p /etc/swanctl/{x509ca,x509,private}
echo '1574a' | sudo -S cp ~/pqgm-test/caCert.pem /etc/swanctl/x509ca/
echo '1574a' | sudo -S cp ~/pqgm-test/responder/responderCert.pem /etc/swanctl/x509/
echo '1574a' | sudo -S cp ~/pqgm-test/responder/responderKey.pem /etc/swanctl/private/
echo "   证书安装完成"
echo ""

# 3. 安装 swanctl 配置
echo "3. 安装 swanctl 配置..."
echo '1574a' | sudo -S tee /etc/swanctl/swanctl.conf >/dev/null <<'EOF'
connections {
    # 传统 IKEv2 基线测试 (2-RTT)
    baseline {
        local_addrs = 192.168.172.131

        local {
            auth = pubkey
            certs = responderCert.pem
            id = "C=CN, O=PQGM-Test, CN=responder.pqgm.test"
        }
        remote {
            auth = pubkey
        }

        children {
            net {
                local_ts = 10.1.2.0/24
                remote_ts = 10.1.1.0/24
                esp_proposals = aes256-sha256-x25519
                start_action = trap
            }
        }

        version = 2
        proposals = aes256-sha256-x25519
    }

    # 混合密钥交换测试 (x25519 + ML-KEM-768)
    pqgm-hybrid {
        local_addrs = 192.168.172.131

        local {
            auth = pubkey
            certs = responderCert.pem
            id = "C=CN, O=PQGM-Test, CN=responder.pqgm.test"
        }
        remote {
            auth = pubkey
        }

        children {
            net {
                local_ts = 10.1.2.0/24
                remote_ts = 10.1.1.0/24
                esp_proposals = aes256gcm16-sha256-x25519-ke1_mlkem768
                start_action = trap
            }
        }

        version = 2
        proposals = aes256-sha256-x25519-ke1_mlkem768
    }
}
EOF
echo "   配置文件安装完成"
echo ""

# 4. 启动 charon
echo "4. 启动 strongSwan charon..."
echo '1574a' | sudo -S pkill charon 2>/dev/null
sleep 1
echo '1574a' | sudo -S /usr/libexec/ipsec/charon &
sleep 2
echo "   charon 已启动"
echo ""

# 5. 加载配置
echo "5. 加载配置..."
echo '1574a' | sudo -S swanctl --load-all
echo ""

# 6. 显示连接状态
echo "=========================================="
echo "  配置完成！"
echo "=========================================="
echo ""
echo "Responder 信息:"
echo "  IP: 192.168.172.131"
echo "  角色: Responder (等待连接)"
echo ""
echo "已加载的连接:"
echo '1574a' | sudo -S swanctl --list-conns
echo ""
echo "监听状态:"
echo '1574a' | sudo -S netstat -tuln | grep -E "500|4500"
echo ""
echo "=========================================="
echo "  Responder 已就绪"
echo "=========================================="
