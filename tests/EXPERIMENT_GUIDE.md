# PQGM-IKEv2 实验完整指南

## 📋 当前状态

### ✅ 已完成配置

| 组件 | 版本/状态 | 说明 |
|------|----------|------|
| strongSwan | 6.0.4 | 已编译安装，含ML-KEM插件 |
| ML 插件 | ✓ 已启用 | 支持ML-KEM-512/768/1024 |
| curve25519 | ✓ 已启用 | 支持x25519密钥交换 |
| 证书 | ✓ 已生成 | CA + Initiator + Responder |
| 连接配置 | ✓ 已加载 | baseline + pqgm-hybrid |

### 配置的连接

```
baseline:       传统 IKEv2 (2-RTT, x25519)
pqgm-hybrid:    混合密钥交换 (3-RTT, x25519 + ML-KEM-768)
```

---

## 🚀 快速开始

### 方案A：克隆VM后双机测试（推荐）

#### 步骤1：在VMware中克隆当前VM

1. 关闭当前VM或创建快照
2. 右键VM → 管理 → 克隆 → 完整克隆
3. 命名为 "PQGM-Responder"

#### 步骤2：配置Responder VM

启动克隆的VM，运行：
```bash
cd ~/pqgm-test
./setup_responder.sh
```

#### 步骤3：运行自动化测试

在 **Initiator VM** (当前机器) 上运行：
```bash
cd ~/pqgm-test
./run_dual_vm_tests.sh
```

测试完成后，结果保存在 `~/pqgm-test/results/` 目录。

---

### 方案B：单机配置验证

在当前VM上运行验证：
```bash
cd ~/pqgm-test
./validate_setup.sh
```

---

## 📊 预期实验结果

### 表5-2：通信开销对比

| 阶段 | 传统 IKEv2 | PQ-GM-IKEv2 | 说明 |
|------|-----------|-------------|------|
| IKE_SA_INIT | ~450 B | ~520 B | 增加ML-KEM Transform ID |
| IKE_INTERMEDIATE | 0 B | ~2580 B | ML-KEM公钥+密文 |
| IKE_AUTH | ~1250 B | ~550 B | 证书已前置，减少负载 |
| **总计** | **~1700 B** | **~3650 B** | 增长约2.1倍 |

### 表5-3：握手时延对比

| 测试场景 | 握手轮次 | 预期时延 |
|----------|----------|----------|
| 传统 IKEv2 | 2-RTT | 20-30 ms |
| PQ-GM-IKEv2 | 3-RTT | 60-100 ms |

---

## 📁 文件结构

```
~/pqgm-test/
├── caCert.pem              # CA证书
├── caKey.pem               # CA私钥
├── initiator/              # Initiator证书目录
│   ├── initiatorCert.pem
│   ├── initiatorKey.pem
│   └── initiatorReq.pem
├── responder/              # Responder证书目录
│   ├── responderCert.pem
│   ├── responderKey.pem
│   └── responderReq.pem
├── validate_setup.sh       # 配置验证脚本
├── setup_responder.sh      # Responder配置脚本
├── run_dual_vm_tests.sh    # 自动化双VM测试
├── test_latencies.sh       # 时延测试脚本
├── capture_pcap.sh         # 抓包分析脚本
├── results/                # 测试结果目录
│   ├── baseline_*.pcap
│   ├── hybrid_*.pcap
│   └── report_*.txt
└── VM_CLONE_GUIDE.md       # 详细克隆指南
```

---

## 🔧 故障排除

### charon未运行
```bash
ps aux | grep charon                    # 检查是否运行
echo '1574a' | sudo -S /usr/libexec/ipsec/charon &   # 启动
```

### 无法连接到Responder
```bash
ping 192.168.172.131                    # 测试连通性
nc -zv 192.168.172.131 500              # 测试IKE端口
```

### 查看连接状态
```bash
echo '1574a' | sudo -S swanctl --list-sas
echo '1574a' | sudo -S swanctl --list-conns
```

### 查看日志
```bash
echo '1574a' | sudo -S journalctl -u strongswan -n 50
```

---

## 📝 数据记录模板

完成测试后，记录数据到论文：

```markdown
### 5.4.1 通信开销分析

| 阶段 | 传统IKEv2 | PQ-GM-IKEv2 | 增长倍数 |
|------|-----------|-------------|----------|
| IKE_SA_INIT | [实际值] B | [实际值] B | [计算值]x |
| IKE_INTERMEDIATE | 0 B | [实际值] B | - |
| IKE_AUTH | [实际值] B | [实际值] B | [计算值]x |
| 总计 | [实际值] B | [实际值] B | [计算值]x |

### 5.4.2 握手时延分析

| 测试场景 | 平均时延 | 标准差 | 样本数 |
|----------|----------|--------|--------|
| 传统 IKEv2 | [实际值] ms | [实际值] ms | 1000 |
| PQ-GM-IKEv2 | [实际值] ms | [实际值] ms | 1000 |
```

---

## ✅ 验证清单

在开始双VM测试前，确保：

- [ ] strongSwan 6.0.4 已安装
- [ ] ML 插件已加载
- [ ] 证书已生成并配置
- [ ] baseline 连接已配置
- [ ] pqgm-hybrid 连接已配置
- [ ] charon 守护进程可启动
- [ ] swanctl 可以加载配置

---

## 🎯 下一步

1. **克隆VM** → 运行 `setup_responder.sh`
2. **运行测试** → 运行 `run_dual_vm_tests.sh`
3. **收集数据** → 检查 `~/pqgm-test/results/`
4. **更新论文** → 将实验数据填入第五章

---

*配置时间: $(date)*
*strongSwan版本: 6.0.4*
*操作系统: Ubuntu 22.04.5 LTS*
