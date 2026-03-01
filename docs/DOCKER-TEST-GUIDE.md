# Docker 双端测试指南

## 配置说明

### 配置文件位置
- **本地Loopback配置**: `configs/loopback/swanctl.conf`
- **Docker Initiator配置**: `configs/docker-initiator/swanctl.conf`
- **Docker Responder配置**: `configs/docker-responder/swanctl.conf`

### 切换配置
```bash
# 切换到本地loopback配置
./scripts/switch-config.sh loopback

# 更新Docker配置
./scripts/switch-config.sh docker
```

## Docker测试步骤

### 1. 启动容器
```bash
cd /home/ipsec/PQGM-IPSec/docker
sudo docker-compose down
sudo docker-compose up -d
```

### 2. 在容器中运行ldconfig
```bash
sudo docker exec pqgm-initiator ldconfig
sudo docker exec pqgm-responder ldconfig
```

### 3. 启动charon
```bash
# Responder
sudo docker exec -d pqgm-responder /usr/local/libexec/ipsec/charon

# Initiator
sudo docker exec -d pqgm-initiator /usr/local/libexec/ipsec/charon
```

### 4. 加载配置
```bash
# 等待charon启动
sleep 3

# Responder
sudo docker exec pqgm-responder /usr/local/sbin/swanctl --load-all

# Initiator
sudo docker exec pqgm-initiator /usr/local/sbin/swanctl --load-all
```

### 5. 发起连接
```bash
sudo docker exec pqgm-initiator /usr/local/sbin/swanctl --initiate --child ipsec
```

### 6. 查看日志
```bash
# Initiator日志
sudo docker exec pqgm-initiator cat /var/log/syslog 2>/dev/null || \
sudo docker logs pqgm-initiator

# Responder日志
sudo docker exec pqgm-responder cat /var/log/syslog 2>/dev/null || \
sudo docker logs pqgm-responder
```

## 一键测试脚本

```bash
#!/bin/bash
# test-docker.sh

set -e

echo "=== Starting Docker containers ==="
cd /home/ipsec/PQGM-IPSec/docker
sudo docker-compose down 2>/dev/null || true
sudo docker-compose up -d

echo "=== Waiting for containers to start ==="
sleep 5

echo "=== Running ldconfig ==="
sudo docker exec pqgm-initiator ldconfig
sudo docker exec pqgm-responder ldconfig

echo "=== Starting charon ==="
sudo docker exec -d pqgm-responder /usr/local/libexec/ipsec/charon
sleep 2
sudo docker exec -d pqgm-initiator /usr/local/libexec/ipsec/charon

echo "=== Waiting for charon ==="
sleep 3

echo "=== Loading configurations ==="
sudo docker exec pqgm-responder /usr/local/sbin/swanctl --load-all
sudo docker exec pqgm-initiator /usr/local/sbin/swanctl --load-all

echo "=== Initiating connection ==="
sudo docker exec pqgm-initiator /usr/local/sbin/swanctl --initiate --child ipsec

echo "=== Test complete ==="
```

## 常见问题

### 1. gmalg插件加载失败
```
plugin 'gmalg' failed to load: libgmssl.so.3: cannot open shared object file
```
**解决方案**: 在容器中运行`ldconfig`

### 2. 证书文件找不到
```
cannot open certificate file /usr/local/etc/swanctl/x509/sm2_sign_cert.pem
```
**解决方案**: 代码现在会自动尝试`sm2_sign_cert.pem`和`signCert.pem`两种文件名

### 3. vici连接被拒绝
```
error: connecting to 'default' URI failed: Connection refused
```
**解决方案**: 确保charon正在运行，并等待几秒钟让vici socket准备好
