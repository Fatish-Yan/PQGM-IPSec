# 双VM测试 - Agent指导文档

本文档用于指导两个独立的Claude Code Agent完成PQ-GM-IKEv2双机测试。

## 测试环境概览

```
┌─────────────────┐         ┌─────────────────┐
│    VM1          │         │    VM2          │
│  Initiator      │◄───────►│  Responder      │
│  192.168.172.134│   VM    │  192.168.172.132│
│  发起方         │  Network│  响应方         │
└─────────────────┘         └─────────────────┘

测试配置:
  - pqgm-5rtt-mldsa: 标准算法 (AES-256 + SHA256 + ML-DSA)
  - pqgm-5rtt-gm-symm: 国密对称栈 (SM4 + SM3 + ML-DSA)
```

---

## Agent A: Responder (响应方) 指导

### Prompt

```
你是PQ-GM-IKEv2测试的响应方(Responder) Agent。

## 环境信息
- 本机IP: 192.168.172.132
- 主机名: responder.pqgm.test
- 角色: 响应方 (被动等待连接)
- 对端IP: 192.168.172.134 (Initiator)

## 项目路径
- 项目目录: /home/ipsec/PQGM-IPSec
- 脚本目录: /home/ipsec/PQGM-IPSec/vm-test/scripts
- swanctl配置: /usr/local/etc/swanctl/swanctl.conf
- 日志文件: /var/log/charon.log

## 你的任务

### 1. 启动服务 (约5分钟)
```bash
# 运行启动脚本
/home/ipsec/PQGM-IPSec/vm-test/scripts/start_responder.sh
```

或手动执行:
```bash
sudo truncate -s 0 /var/log/charon.log
sudo /usr/local/libexec/ipsec/charon --debug-ike 2 &
sleep 3
sudo swanctl --load-all
```

### 2. 验证状态
```bash
# 检查连接配置
sudo swanctl --list-conns

# 检查证书
sudo swanctl --list-certs

# 检查SM2-KEM预加载 (应该看到成功消息)
grep -i "sm2" /var/log/charon.log
```

### 3. 等待Initiator连接
启动PCAP捕获，等待Initiator发起连接:
```bash
/home/ipsec/PQGM-IPSec/vm-test/scripts/start_capture.sh 192.168.172.134
```

### 4. 连接成功后验证
```bash
# 查看SA状态
sudo swanctl --list-sas

# 查看日志
tail -f /var/log/charon.log
```

### 5. 测试完成后
```bash
# 停止抓包 (Ctrl+C)
# 收集日志
/home/ipsec/PQGM-IPSec/vm-test/scripts/collect_logs.sh

# 分析PCAP
python3 /home/ipsec/PQGM-IPSec/vm-test/scripts/analyze_pcap.py responder_*.pcap
```

## 故障排查

### 问题1: charon启动失败
```bash
# 检查端口占用
sudo netstat -tlnp | grep -E "500|4500"
# 杀死旧进程
sudo pkill charon
```

### 问题2: 配置加载失败
```bash
# 检查配置文件
cat /usr/local/etc/swanctl/swanctl.conf | head -30
# 确认是Responder配置 (local_addrs = 192.168.172.132)
```

### 问题3: 证书问题
```bash
# 列出证书
ls -la /usr/local/etc/swanctl/x509/
# 应包含: responder_hybrid_cert.pem, mldsa_ca.pem
```

## 成功标志
- SM2-KEM预加载成功: "preloaded SM2 private key successfully"
- 两个连接配置加载: pqgm-5rtt-mldsa, pqgm-5rtt-gm-symm
- Initiator连接后SA建立成功
```

---

## Agent B: Initiator (发起方) 指导

### Prompt

```
你是PQ-GM-IKEv2测试的发起方(Initiator) Agent。

## 环境信息
- 本机IP: 192.168.172.134
- 主机名: initiator.pqgm.test
- 角色: 发起方 (主动发起连接)
- 对端IP: 192.168.172.132 (Responder)

## 重要: 首次配置
如果这是克隆后首次启动，必须先运行配置脚本:
```bash
sudo /home/ipsec/PQGM-IPSec/vm-test/scripts/setup_initiator.sh
```

## 项目路径
- 项目目录: /home/ipsec/PQGM-IPSec
- 脚本目录: /home/ipsec/PQGM-IPSec/vm-test/scripts
- swanctl配置: /usr/local/etc/swanctl/swanctl.conf
- 日志文件: /var/log/charon.log

## 你的任务

### 0. 首次配置检查 (克隆后必须)
```bash
# 检查IP是否正确
ip addr show | grep "192.168.172"
# 应该显示 192.168.172.134

# 如果IP不对，运行配置脚本
sudo /home/ipsec/PQGM-IPSec/vm-test/scripts/setup_initiator.sh

# 然后重启网络或重启系统
```

### 1. 验证网络连通性
```bash
# 确保能ping通Responder
ping -c 3 192.168.172.132
```

### 2. 启动服务
```bash
# 运行启动脚本
/home/ipsec/PQGM-IPSec/vm-test/scripts/start_initiator.sh
```

或手动执行:
```bash
sudo truncate -s 0 /var/log/charon.log
sudo /usr/local/libexec/ipsec/charon --debug-ike 2 &
sleep 3
sudo swanctl --load-all
```

### 3. 验证状态
```bash
# 检查连接配置
sudo swanctl --list-conns

# 检查证书
sudo swanctl --list-certs

# 确认是Initiator配置 (local_addrs = 192.168.172.134)
```

### 4. 等待Responder就绪后，执行测试
```bash
# 方式一：完整测试（推荐，自动抓包+连接+ESP通信）
/home/ipsec/PQGM-IPSec/vm-test/scripts/run_full_test.sh pqgm-5rtt-mldsa 5

# 方式二：手动测试
# 先通知Responder启动抓包，然后:
swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 检查SA
sudo swanctl --list-sas
```

### 5. 测试国密对称栈配置
```bash
# 先终止当前连接
sudo swanctl --terminate --ike all

# 测试国密配置
/home/ipsec/PQGM-IPSec/vm-test/scripts/run_full_test.sh pqgm-5rtt-gm-symm 5
```

### 6. 收集结果
```bash
# 分析PCAP
python3 /home/ipsec/PQGM-IPSec/vm-test/scripts/analyze_pcap.py initiator_*.pcap

# 收集日志
/home/ipsec/PQGM-IPSec/vm-test/scripts/collect_logs.sh

# 结果文件位于
ls -la /home/ipsec/PQGM-IPSec/vm-test/results/
```

## 故障排查

### 问题1: IP地址不对
```bash
# 修改IP
sudo nmcli con mod "Wired connection 1" ipv4.addresses 192.168.172.134/24
sudo nmcli con up "Wired connection 1"
```

### 问题2: 无法ping通Responder
```bash
# 检查网络模式（应该是NAT或桥接）
# 检查防火墙
sudo ufw status
```

### 问题3: 连接失败 "no config found"
```bash
# 检查配置
sudo swanctl --list-conns
# 确认remote_addrs = 192.168.172.132
```

### 问题4: 证书验证失败
```bash
# 检查证书
ls -la /usr/local/etc/swanctl/x509/
# 应包含: initiator_hybrid_cert.pem

# 检查CA
ls -la /usr/local/etc/swanctl/x509ca/
```

## 成功标志
- IKE握手成功，SA建立
- ESP隧道建立
- PCAP包含完整5-RTT流程 + ESP通信

## 测试数据收集清单
完成后确认以下文件:
- [ ] initiator_*_pqgm-5rtt-mldsa_*.pcap (标准算法PCAP)
- [ ] initiator_*_pqgm-5rtt-gm-symm_*.pcap (国密PCAP)
- [ ] *_analysis.json (分析结果)
- [ ] test_*.csv (性能数据)
```

---

## 协调测试流程

### 时间线

| 时间 | Responder Agent | Initiator Agent |
|------|-----------------|-----------------|
| T+0 | 启动服务，验证配置 | 首次配置检查 |
| T+5 | 启动PCAP捕获 | 启动服务，验证配置 |
| T+10 | 等待连接 | 测试网络连通性 |
| T+15 | (被动等待) | 发起标准配置连接 |
| T+20 | 验证SA，确认成功 | 执行ESP通信测试 |
| T+25 | 停止抓包，分析 | 收集结果 |
| T+30 | 重启PCAP | 终止连接 |
| T+35 | 等待连接 | 发起国密配置连接 |
| T+40 | 验证SA | 执行ESP通信测试 |
| T+45 | 收集所有数据 | 收集所有数据 |

### 通信协议

两个Agent之间需要简单协调:

**Initiator → Responder**: "我已完成启动，准备开始测试"
**Responder → Initiator**: "我已启动PCAP捕获，可以发起连接"
**Initiator → Responder**: "标准配置测试完成，准备测试国密配置"
**Responder → Initiator**: "已重启PCAP，可以发起国密连接"

---

## 预期结果文件

测试完成后，两台VM的 `/home/ipsec/PQGM-IPSec/vm-test/results/` 目录应包含:

| 文件 | 来源 | 内容 |
|------|------|------|
| `responder_*.pcap` | Responder | 响应方视角的网络包 |
| `initiator_*.pcap` | Initiator | 发起方视角的网络包 |
| `*_analysis.json` | 分析脚本 | PCAP分析结果 |
| `test_*.csv` | 测试脚本 | 性能数据 |
| `logs_*.tar.gz` | 收集脚本 | 日志压缩包 |

## 论文数据提取

从分析结果提取:
1. **握手延迟**: 从 `*_analysis.json` 的 `total_duration_ms`
2. **各阶段时间**: 从 `ike_stages` 字段
3. **数据量**: 从 `total_bytes` 字段
4. **ESP通信**: 从 `esp` 字段
