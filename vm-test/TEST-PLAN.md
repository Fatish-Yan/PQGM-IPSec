# PQ-GM-IKEv2 双VM测试执行计划

## 测试目标

1. 在真实双VM环境中验证PQ-GM-IKEv2协议
2. 收集论文所需的性能数据
3. 获取PCAP数据包用于协议分析
4. 对比不同配置的性能差异

## 测试环境

| 项目 | VM1 (Initiator) | VM2 (Responder) |
|------|-----------------|-----------------|
| IP地址 | 192.168.172.134 | 192.168.172.132 |
| 主机名 | initiator.pqgm.test | responder.pqgm.test |
| 角色 | 发起方 | 响应方 |

## 测试用例

| 编号 | 配置名 | IKE加密 | IKE完整性 | PRF | 认证 | 说明 |
|------|--------|---------|-----------|-----|------|------|
| T1 | pqgm-5rtt-mldsa | AES-256-CBC | HMAC-SHA256 | SHA256 | ML-DSA-65 | 标准算法基准 |
| T2 | pqgm-5rtt-gm-symm | SM4-CBC | HMAC-SM3 | SM3 | ML-DSA-65 | 国密对称栈 |

## PCAP捕获范围

抓包脚本捕获完整的IKEv2 + ESP通信流程：

| 阶段 | 协议/端口 | 说明 |
|------|----------|------|
| IKE_SA_INIT | UDP 500 | 初始密钥交换 (x25519 + ML-KEM + SM2-KEM协商) |
| IKE_INTERMEDIATE #0 | UDP 4500 | 双证书分发 (SM2 SignCert + EncCert) |
| IKE_INTERMEDIATE #1 | UDP 4500 | SM2-KEM 密钥交换 |
| IKE_INTERMEDIATE #2 | UDP 4500 | ML-KEM-768 密钥交换 |
| IKE_AUTH | UDP 4500 | ML-DSA-65 签名认证 |
| ESP数据通信 | 协议50 或 UDP 4500 | IPsec隧道数据传输 |

tcpdump过滤表达式：
```
host <对端IP> and (udp port 500 or udp port 4500 or proto 50)
```

## 执行步骤

### Phase 0: 环境准备 (一次性)

1. **克隆VM**
   - 在VMware中克隆当前VM
   - 生成新的MAC地址
   - 命名为 PQGM-IPSec-Initiator

2. **配置Responder (当前机器)**
   ```bash
   # 运行配置脚本
   sudo /home/ipsec/PQGM-IPSec/vm-test/scripts/setup_responder.sh

   # 验证配置
   swanctl --list-certs
   ```

3. **配置Initiator (克隆机器)**
   ```bash
   # 运行配置脚本
   sudo /home/ipsec/PQGM-IPSec/vm-test/scripts/setup-initiator-vm.sh

   # 验证网络
   ping 192.168.172.132

   # 验证配置
   swanctl --list-certs
   ```

### Phase 1: 环境验证

**在两台VM上执行**:
```bash
# 1. 清空日志
sudo truncate -s 0 /var/log/charon.log

# 2. 启动strongSwan
sudo systemctl restart strongswan

# 3. 加载配置
swanctl --load-all

# 4. 检查证书
swanctl --list-certs
```

**网络连通性测试**:
```bash
# Initiator上
ping 192.168.172.132

# Responder上
ping 192.168.172.134
```

### Phase 2: 连接测试

**先启动Responder**:
```bash
# 在VM2上
sudo systemctl restart strongswan
swanctl --load-all
```

**再启动Initiator并测试**:
```bash
# 在VM1上
sudo systemctl restart strongswan
swanctl --load-all

# 测试标准配置
swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 检查SA
swanctl --list-sas
```

### Phase 3: 性能数据收集

**手动分步操作（推荐）**

**启动PCAP捕获 (两台VM同时)**:
```bash
# 在两台VM上分别运行
# 捕获范围：IKE (UDP 500/4500) + ESP (协议50 + UDP 4500封装)
/home/ipsec/PQGM-IPSec/vm-test/scripts/start_capture.sh <对端IP>
```

**执行连接测试 (在Initiator上)**:
```bash
# 标准算法测试
swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 国密对称栈测试 (先断开上一个)
swanctl --terminate --ike all
swanctl --initiate --child net --ike pqgm-5rtt-gm-symm
```

**执行ESP通信测试**（确保捕获ESP流量）:
```bash
# 连接成功后，通过隧道发送数据
# 方法1: ping隧道对端内网地址
ping -c 5 10.2.0.1

# 方法2: iperf3测试（如果已安装）
# iperf3 -c 10.2.0.1
```

**停止PCAP捕获**:
- 按 Ctrl+C 停止tcpdump

### Phase 4: 数据收集

**收集日志 (两台VM)**:
```bash
/home/ipsec/PQGM-IPSec/vm-test/scripts/collect_logs.sh
```

**分析PCAP**:
```bash
python3 /home/ipsec/PQGM-IPSec/vm-test/scripts/analyze_pcap.py initiator_*.pcap
python3 /home/ipsec/PQGM-IPSec/vm-test/scripts/analyze_pcap.py responder_*.pcap
```

### Phase 5: 数据汇总

所有结果文件位于 `/home/ipsec/PQGM-IPSec/vm-test/results/`:

| 文件 | 内容 |
|------|------|
| `test_*.csv` | 性能测试结果 |
| `*_*.pcap` | 网络数据包 |
| `logs_*.tar.gz` | 日志压缩包 |
| `*_analysis.json` | PCAP分析结果 |

## 预期论文数据

### 表1: 握手延迟对比

| 配置 | 平均延迟(ms) | 最小(ms) | 最大(ms) |
|------|-------------|---------|---------|
| pqgm-5rtt-mldsa | ? | ? | ? |
| pqgm-5rtt-gm-symm | ? | ? | ? |

### 表2: 数据包统计

| 配置 | 总包数 | 总字节数 | Initiator包 | Responder包 |
|------|--------|----------|-------------|-------------|
| pqgm-5rtt-mldsa | ? | ? | ? | ? |
| pqgm-5rtt-gm-symm | ? | ? | ? | ? |

### 表3: 各RTT时间分解

从PCAP分析提取5个RTT的时间:
- RTT1: IKE_SA_INIT
- RTT2: IKE_INTERMEDIATE #0
- RTT3: IKE_INTERMEDIATE #1
- RTT4: IKE_INTERMEDIATE #2
- RTT5: IKE_AUTH

## 故障排除

### 问题1: 无法ping通
```bash
# 检查网络模式
# VMware应该使用NAT或桥接模式
# 两台VM应该在同一网段
```

### 问题2: 连接失败 "no config found"
```bash
# 检查配置是否加载
swanctl --list-conns

# 重新加载
swanctl --load-all
```

### 问题3: 证书验证失败
```bash
# 检查CA证书
swanctl --list-certs --type x509ca

# 检查证书ID
openssl x509 -in /usr/local/etc/swanctl/x509/initiator_hybrid_cert.pem -text -noout | grep Subject
```

### 问题4: ML-DSA签名失败
```bash
# 检查ML-DSA插件
swanctl --stats

# 检查日志
tail -f /var/log/charon.log
```

## 测试检查清单

- [ ] VM克隆完成
- [ ] Responder配置完成
- [ ] Initiator配置完成
- [ ] 网络连通性验证
- [ ] strongSwan启动成功
- [ ] 证书加载成功
- [ ] 标准配置连接成功
- [ ] 国密配置连接成功
- [ ] PCAP捕获完成
- [ ] 性能测试完成
- [ ] 日志收集完成
- [ ] 数据汇总完成
