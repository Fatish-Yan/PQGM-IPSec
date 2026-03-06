# Responder 端配置部署说明

## 文件列表

```
responder-config/
├── swanctl.conf              # 主配置文件
├── ecdsa_ca.pem              # ECDSA CA 证书 (用于标准 IKEv2)
├── responder_ecdsa_cert.pem  # Responder ECDSA 证书
├── responder_ecdsa_key.pem   # Responder ECDSA 私钥
├── mldsa_ca.pem              # ML-DSA CA 证书 (用于后量子提案)
├── responder_hybrid_cert.pem # Responder ML-DSA 混合证书
└── responder_mldsa_key.bin   # Responder ML-DSA 私钥
```

## 部署步骤

### 1. 复制证书文件

```bash
# CA 证书
sudo cp ecdsa_ca.pem /usr/local/etc/swanctl/x509ca/
sudo cp mldsa_ca.pem /usr/local/etc/swanctl/x509ca/

# 端证书
sudo cp responder_ecdsa_cert.pem /usr/local/etc/swanctl/x509/
sudo cp responder_hybrid_cert.pem /usr/local/etc/swanctl/x509/

# 私钥
sudo cp responder_ecdsa_key.pem /usr/local/etc/swanctl/private/
sudo cp responder_mldsa_key.bin /usr/local/etc/swanctl/private/
sudo chmod 600 /usr/local/etc/swanctl/private/*.pem
sudo chmod 600 /usr/local/etc/swanctl/private/*.bin
```

### 2. 更新配置文件

```bash
sudo cp swanctl.conf /usr/local/etc/swanctl/swanctl.conf
```

### 3. 重启 strongSwan

```bash
# 停止现有 charon 进程
sudo pkill charon

# 启动 charon
sudo /usr/local/libexec/ipsec/charon --debug-ike 2 &

# 加载配置
sudo swanctl --load-all
```

### 4. 验证配置

```bash
# 检查已加载的连接
sudo swanctl --list-conns

# 应该看到 4 个连接:
# - standard-ikev2
# - pq-3rtt-mlkem
# - pqgm-5rtt-mldsa
# - pqgm-5rtt-gm-symm
```

## 测试等待

Responder 配置完成后，等待 Initiator 发起连接测试。
