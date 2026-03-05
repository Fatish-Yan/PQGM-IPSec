# PQ-GM-IKEv2 VM 双机测试指南

> 本目录包含 PQ-GM-IKEv2 协议双机测试所需的所有配置文件、脚本和文档

---

## 快速开始

### Docker 测试 (推荐)

```bash
# 一键测试
sudo ./scripts/quick-docker-test.sh
```

### VM 双机测试

1. **克隆 VM** - 从当前 Responder VM 克隆一个 Initiator VM
2. **配置 Initiator** - 在克隆的 VM 上运行配置脚本
3. **执行测试** - 两端配合完成测试

---

## 目录结构

```
vm-test/
├── README.md                      # 本文档
├── INITIATOR-SETUP-GUIDE.md       # Initiator 详细配置指南
├── INITIATOR-QUICK-REF.md         # Initiator 快速参考卡片
├── TEST-PLAN.md                   # 测试执行计划
│
├── docker/                        # Docker 专用配置
│   ├── initiator/
│   │   └── swanctl.conf           # Docker Initiator (172.30.0.10)
│   └── responder/
│       └── swanctl.conf           # Docker Responder (172.30.0.20)
│
├── initiator/                     # VM Initiator 配置
│   └── swanctl.conf               # VM Initiator (192.168.172.134)
│
├── responder/                     # VM Responder 配置
│   └── swanctl.conf               # VM Responder (192.168.172.132)
│
├── scripts/                       # 脚本目录
│   ├── quick-docker-test.sh       # 一键 Docker 测试
│   ├── setup-initiator-vm.sh      # Initiator VM 配置脚本
│   ├── setup_responder.sh         # Responder VM 配置脚本
│   ├── start_capture.sh           # PCAP 捕获
│   ├── collect_logs.sh            # 日志收集
│   └── analyze_pcap.py            # PCAP 分析
│
├── results/                       # 测试结果存放目录
│
├── docker-compose-test.yml        # Docker Compose 配置
├── gmalg.conf                     # gmalg 插件配置
└── strongswan.conf                # strongSwan 配置
```

---

## 网络拓扑

### Docker 测试环境

```
┌─────────────────────┐                    ┌─────────────────────┐
│  pqgm-initiator-test│                    │  pqgm-responder-test│
│  172.30.0.10        │◄──────────────────►│  172.30.0.20        │
│  initiator.pqgm.test│   Docker Bridge    │  responder.pqgm.test│
│  10.1.0.0/16 (VPN)  │    172.30.0.0/24   │  10.2.0.0/16 (VPN)  │
└─────────────────────┘                    └─────────────────────┘
```

### VM 双机测试环境

```
┌─────────────────────┐                    ┌─────────────────────┐
│    VM1              │                    │    VM2              │
│  Initiator          │◄──────────────────►│  Responder          │
│  192.168.172.134    │   VM Network       │  192.168.172.132    │
│  initiator.pqgm.test│                    │  responder.pqgm.test│
│  10.1.0.0/16 (VPN)  │                    │  10.2.0.0/16 (VPN)  │
└─────────────────────┘                    └─────────────────────┘
```

---

## 5-RTT 协议流程

| RTT | 阶段 | 说明 |
|-----|------|------|
| 1 | IKE_SA_INIT | 协商三重密钥交换 (x25519 + SM2-KEM + ML-KEM-768) |
| 2 | IKE_INTERMEDIATE #0 | 双证书分发 (SM2 SignCert + EncCert) |
| 3 | IKE_INTERMEDIATE #1 | SM2-KEM 密钥交换 (141 字节密文) |
| 4 | IKE_INTERMEDIATE #2 | ML-KEM-768 密钥交换 (分片传输) |
| 5 | IKE_AUTH | ML-DSA-65 后量子签名认证 (3309 字节签名) |

---

## 测试配置

| 配置名 | IKE 加密 | PRF | 认证 | 用途 |
|--------|----------|-----|------|------|
| `pqgm-5rtt-mldsa` | AES-256-CBC + HMAC-SHA256 | SHA256 | ML-DSA-65 | 标准算法基准 |
| `pqgm-5rtt-gm-symm` | SM4-CBC + HMAC-SM3 | SM3 | ML-DSA-65 | 国密对称栈 |

---

## 使用指南

### 场景 1: Docker 快速验证

适合：验证协议实现是否正确

```bash
cd /home/ipsec/PQGM-IPSec/vm-test
sudo ./scripts/quick-docker-test.sh
```

### 场景 2: VM 双机测试 (从零开始)

适合：收集论文数据、演示

#### 步骤 1: 克隆 VM

1. 在 VMware 中克隆当前 VM (完整克隆)
2. 生成新的 MAC 地址
3. 命名为 `PQGM-IPSec-Initiator`

#### 步骤 2: 配置 Initiator VM

在克隆的 Initiator VM 上执行：

```bash
# 查看详细指南
cat /home/ipsec/PQGM-IPSec/vm-test/INITIATOR-SETUP-GUIDE.md

# 或使用一键配置脚本
sudo /home/ipsec/PQGM-IPSec/vm-test/scripts/setup-initiator-vm.sh
```

#### 步骤 3: 启动 Responder

在当前 VM (Responder) 上执行：

```bash
# 停止可能运行的 charon
sudo pkill charon

# 启动 charon
sudo /usr/local/libexec/ipsec/charon --debug-ike 2 &

# 加载配置
swanctl --load-all
```

#### 步骤 4: 发起连接

在 Initiator VM 上执行：

```bash
swanctl --initiate --child net --ike pqgm-5rtt-mldsa
```

### 场景 3: 收集论文数据

```bash
# 1. 在两端启动 PCAP 捕获
sudo ./scripts/start_capture.sh

# 2. 执行测试
swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 3. 收集日志
./scripts/collect_logs.sh

# 4. 分析 PCAP
python3 ./scripts/analyze_pcap.py /tmp/pqgm-*.pcap
```

---

## 关键配置约定

### 文件命名

| 类型 | 命名规则 | 说明 |
|------|----------|------|
| SM2 加密私钥 | `enc_key.pem` | **必须**是这个文件名 |
| SM2 私钥密码 | `PQGM2026` | 固定密码 |
| ML-DSA 混合证书 | `*_hybrid_cert.pem` | ECDSA 占位符 + ML-DSA 扩展 |
| ML-DSA 私钥 | `*_mldsa_key.bin` | 4032 字节二进制 |

### 算法 ID

| 算法 | ID | 说明 |
|------|-----|------|
| KE_SM2 (SM2-KEM) | 1051 | gmalg 插件 |
| PRF_SM3 | 1052 | gmalg 插件 |
| AUTH_MLDSA_65 | 1053 | mldsa 插件 |

### 提案字符串

```
aes256-sha256-x25519-ke1_sm2kem-ke2_mlkem768
│      │      │       │          │
│      │      │       │          └── ADDKE2: ML-KEM-768
│      │      │       └── ADDKE1: SM2-KEM
│      │      └── KE: x25519
│      └── PRF/Integrity: HMAC-SHA256
└── Encryption: AES-256-CBC
```

---

## 常见问题

### Q1: 私钥加载失败

确保私钥文件名和权限正确：
```bash
sudo chmod 600 /usr/local/etc/swanctl/private/*
```

### Q2: 证书验证失败

ML-DSA 混合证书使用实验性绕过，确保已应用 auth_cfg.c 补丁。

### Q3: SM2-KEM 解密失败

检查私钥密码配置：
```bash
cat /usr/local/etc/strongswan.d/charon/gmalg.conf
# 确保 enc_key_secret = PQGM2026
```

### Q4: 网络不通

```bash
# 检查 IP
ip addr

# 检查防火墙
sudo ufw allow 500/udp
sudo ufw allow 4500/udp
```

---

## 相关文档

- [INITIATOR-SETUP-GUIDE.md](./INITIATOR-SETUP-GUIDE.md) - Initiator 详细配置指南
- [INITIATOR-QUICK-REF.md](./INITIATOR-QUICK-REF.md) - Initiator 快速参考卡片
- [TEST-PLAN.md](./TEST-PLAN.md) - 测试执行计划
- [../docs/DOCKER-TEST-MANUAL.md](../docs/DOCKER-TEST-MANUAL.md) - Docker 测试完整手册

---

*最后更新: 2026-03-05*
