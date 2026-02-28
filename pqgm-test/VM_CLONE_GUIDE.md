# PQGM-IKEv2 实验环境配置指南

## 当前状态

✅ **Initiator (当前VM)** - IP: 192.168.172.130
- strongSwan 6.0.4 已安装
- ML-KEM 插件已启用
- 证书已配置
- 连接配置已加载:
  - `baseline`: 传统 IKEv2 (x25519)
  - `pqgm-hybrid`: 混合密钥交换 (x25519 + ML-KEM-768)

## 下一步：克隆虚拟机

### 步骤 1: 在 VMware 中克隆当前 VM

1. 关闭当前虚拟机（或创建快照后克隆）
2. 右键虚拟机 → 管理 → 克隆
3. 选择"创建完整克隆"
4. 命名为 "PQGM-Responder"

### 步骤 2: 配置 Responder VM

1. 启动克隆的 VM
2. 修改网络配置:
   ```bash
   sudo ip addr del 192.168.172.130/24 dev ens33
   sudo ip addr add 192.168.172.131/24 dev ens33
   ```

3. 运行 Responder 配置脚本:
   ```bash
   cd ~/pqgm-test
   chmod +x setup_responder.sh
   ./setup_responder.sh
   ```

### 步骤 3: 启动测试

在 **Initiator** (192.168.172.130) 上运行:

```bash
cd ~/pqgm-test

# 启动 charon（如果未运行）
echo '1574a' | sudo -S /usr/libexec/ipsec/charon &
sleep 2

# 测试基线连接
echo '1574a' | sudo -S swanctl --initiate --child baseline

# 测试混合密钥交换连接
echo '1574a' | sudo -S swanctl --initiate --child pqgm-hybrid

# 查看连接状态
echo '1574a' | sudo -S swanctl --list-sas
```

## 测试脚本

### 1. 握手时延测试 (表5-3数据)

```bash
cd ~/pqgm-test
chmod +x test_latencies.sh
./test_latencies.sh
```

### 2. 通信开销测试 (表5-2数据)

```bash
cd ~/pqgm-test
chmod +x capture_pcap.sh
./capture_pcap.sh
```

## 预期结果

### 表 5-2: 协议通信开销对比

| 阶段 | 传统 IKEv2 | PQ-GM-IKEv2 |
|------|-----------|-------------|
| IKE_SA_INIT | ~450 Bytes | ~520 Bytes |
| IKE_INTERMEDIATE (ML-KEM) | 0 | ~2580 Bytes |
| IKE_AUTH | ~1250 Bytes | ~550 Bytes |
| **总计** | **~1700 Bytes** | **~3650 Bytes** |

### 表 5-3: 协议握手时延对比

| 测试场景 | 握手轮次 | 预期时延 |
|----------|----------|----------|
| 传统 IKEv2 | 2-RTT | ~20-30 ms |
| PQ-GM-IKEv2 | 3-RTT | ~60-100 ms |

## 故障排除

### charon 无法启动

```bash
# 检查是否已运行
ps aux | grep charon

# 手动启动
echo '1574a' | sudo -S /usr/libexec/ipsec/charon &
```

### 连接失败

```bash
# 检查证书
echo '1574a' | sudo -S swanctl --list-certs

# 检查连接
echo '1574a' | sudo -S swanctl --list-conns

# 查看日志
echo '1574a' | sudo -S journalctl -u strongswan -n 50
```

### 网络问题

```bash
# 测试连通性
ping 192.168.172.131

# 测试端口
telnet 192.168.172.131 500

# 抓包调试
echo '1574a' | sudo -S tcpdump -i ens33 -n port 500 or port 4500
```

## 文件位置

- 证书目录: `/etc/swanctl/`
- 配置文件: `/etc/swanctl/swanctl.conf`
- 测试脚本: `~/pqgm-test/`
- 抓包文件: `~/pqgm-test/captures/`
