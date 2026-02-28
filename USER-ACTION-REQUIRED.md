# 用户待办事项 (User Action Required)

## M5 模块完成后需要用户参与的事项

### 重要说明

M5 模块的核心实现已经完成，但以下项目需要您的参与才能完成最终验证和论文数据收集。

---

## 一、双机部署测试 (REQUIRED - 优先级：高)

### 1. 准备两台虚拟机

您需要两台 VMware Ubuntu 虚拟机：
- **Initiator VM**: IP 192.168.1.10 (或您的实际 IP)
- **Responder VM**: IP 192.168.1.20 (或您的实际 IP)

### 2. 部署步骤

**在发起方 VM (192.168.1.10) 上：**

```bash
# 1. 进入项目目录
cd /home/ipsec/PQGM-IPSec/.worktrees/m5-protocol-integration

# 2. 复制配置文件
sudo cp configs/initiator/swanctl.conf /etc/swanctl/swanctl.conf
sudo mkdir -p /etc/swanctl/{x509,x509ca,private}
sudo cp configs/initiator/x509/*.pem /etc/swanctl/x509/
sudo cp configs/initiator/x509ca/*.pem /etc/swanctl/x509ca/
sudo cp configs/initiator/private/*.pem /etc/swanctl/private/

# 3. 加载配置
sudo swanctl --load-all

# 4. 启动 strongSwan
sudo systemctl start strongswan-starter
sudo systemctl enable strongswan-starter

# 5. 验证状态
sudo swanctl --stats
```

**在响应方 VM (192.168.1.20) 上：**

```bash
# 同样的步骤，但使用 responder 配置
cd /home/ipsec/PQGM-IPSec/.worktrees/m5-protocol-integration
sudo cp configs/responder/swanctl.conf /etc/swanctl/swanctl.conf
sudo mkdir -p /etc/swanctl/{x509,x509ca,private}
sudo cp configs/responder/x509/*.pem /etc/swanctl/x509/
sudo cp configs/responder/x509ca/*.pem /etc/swanctl/x509ca/
sudo cp configs/responder/private/*.pem /etc/swanctl/private/
sudo swanctl --load-all
sudo systemctl start strongswan-starter
```

### 3. 如果 IP 地址不同

编辑 `/etc/swanctl/swanctl.conf` 中的 IP 地址：
- `local_addrs`: 本机 IP
- `remote_addrs`: 对端 IP

---

## 二、端到端连接测试 (REQUIRED - 优先级：高)

### 1. 发起连接

**在发起方 VM 上：**

```bash
cd /home/ipsec/PQGM-IPSec/.worktrees/m5-protocol-integration
sudo ./scripts/test_pqgm_ikev2.sh all
```

### 2. 检查连接状态

```bash
# 查看 IKE SA
sudo swanctl --list-sas

# 查看 IPsec SA
sudo ip xfrm state

# 查看日志
sudo journalctl -u strongswan-starter -f
```

### 3. 预期结果

成功的连接应该显示：
```
pqgm-ikev2: #1, ESTABLISHED, IKEv2
  local  '192.168.1.10' @ 192.168.1.10[500]
  remote '192.168.1.20' @ 192.168.1.20[500]
  ...
  pqgm-ikev2: #1, reqid 1, INSTALLED, TUNNEL, ESP:AES_GCM_16-256
```

### 4. 查看关键日志

```bash
# 检查三重密钥交换
sudo journalctl -u strongswan-starter | grep -i "ke1\|ke2\|ADDKE"

# 检查证书分发
sudo journalctl -u strongswan-starter | grep -i "PQ-GM-IKEv2.*cert"

# 检查 IKE_INTERMEDIATE
sudo journalctl -u strongswan-starter | grep -i "IKE_INTERMEDIATE"
```

---

## 三、性能基准测试 (REQUIRED - 优先级：高)

这是为论文收集实验数据的关键步骤。

### 1. 运行基准测试

```bash
cd /home/ipsec/PQGM-IPSec/.worktrees/m5-protocol-integration
sudo ./scripts/benchmark_pqgm.sh all
```

### 2. 收集数据

测试结果保存在 `results/` 目录：
- `benchmark_results.csv` - CSV 格式的结果汇总
- `algo_bench_*.log` - 算法性能数据
- `ike_capture_*.pcap` - 数据包捕获文件

### 3. 需要记录的数据

为论文准备以下数据：

| 测试项 | 数据点 | 说明 |
|--------|--------|------|
| IKE SA 建立时间 | 毫秒 | 从 IKE_SA_INIT 到 IKE_AUTH 完成 |
| RTT | 消息往返次数 | 应该是 5 (传统 2) |
| 密钥交换时延 | 毫秒 | 单次密钥交换的平均时间 |
| 数据包大小 | 字节 | 各阶段数据包大小 |
| 吞吐量 | Mbps | 使用 iperf3 测试 |

### 4. 对比测试

建议运行对比测试：
1. **传统 IKEv2** (仅 x25519)
2. **混合 KE** (x25519 + ML-KEM-768)
3. **三重 KE** (x25519 + ML-KEM-768 + SM2-KEM)

---

## 四、故障排查 (如果测试失败)

### 问题 1: 无法建立连接

```bash
# 检查配置语法
sudo swanctl --load-all --config /etc/swanctl/swanctl.conf

# 检查证书
sudo openssl x509 -in /etc/swanctl/x509/sign_cert.pem -text -noout

# 检查防火墙
sudo iptables -L -v -n

# 允许 IKE 和 ESP 流量
sudo iptables -A INPUT -p udp --dport 500 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 4500 -j ACCEPT
sudo iptables -A INPUT -p esp -j ACCEPT
```

### 问题 2: 证书不发送

检查日志中的错误：
```bash
sudo journalctl -u strongswan-starter | grep -i "cert\|certificate"
```

确认 EncCert 有正确的 EKU：
```bash
openssl x509 -in /etc/swanctl/x509/enc_cert.pem -text | grep -i "extended"
```

应该显示 `ikeIntermediate` 扩展密钥用途。

### 问题 3: ADDKE 不执行

确认插件加载：
```bash
sudo swanctl --stats | grep -E "gmalg|ml"
```

应该看到 gmalg 和 ml 插件已加载。

---

## 五、论文相关 (可选但建议)

### 1. 更新第五章数据

根据测试结果更新 `第五章 系统实现与性能评估.docx`：
- 替换虚构的性能数据
- 添加实际的测试截图
- 更新时延对比表

### 2. 准备图表

可能需要准备的图表：
- IKEv2 消息序列图 (5 RTT vs 2 RTT)
- 性能对比柱状图
- 数据包大小对比图

### 3. 记录实验环境

- CPU: `lscpu`
- 内存: `free -h`
- 网络: `ip addr`
- strongSwan 版本: `swanctl --version`

---

## 六、代码合并 (完成后)

测试成功后，将 worktree 合并回主分支：

```bash
# 在主项目目录
cd /home/ipsec/PQGM-IPSec
git checkout main
git merge m5-protocol-integration
```

---

## 快速检查清单

- [ ] 两台 VM 能互相 ping 通
- [ ] strongSwan 在两台 VM 上都启动
- [ ] `swanctl --list-sas` 显示 ESTABLISHED
- [ ] `ip xfrm state` 显示 IPsec SA
- [ ] 日志中有 "PQ-GM-IKEv2" 相关消息
- [ ] 日志中有 ADDKE 相关消息
- [ ] benchmark 数据已收集

---

## 联系和反馈

如有问题或测试失败，请提供：
1. `sudo swanctl --list-sas` 的输出
2. `sudo journalctl -u strongswan-starter -n 100` 的输出
3. `sudo ip xfrm state` 的输出
4. 错误截图或日志文件

---

祝测试顺利！Good luck! 🚀
