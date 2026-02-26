# PQGM-IKEv2: 抗量子IPSec协议设计与实现

## 项目简介

本项目实现了 **PQ-GM-IKEv2 协议**，这是一种结合了**后量子密码学**和**国密算法**的增强型 IKEv2 协议。该协议为 IPsec VPN 提供了抗量子计算攻击的安全保护。

### 核心特性

- **SM2 双证书机制**: 签名证书 + 加密证书分离
- **SM2-KEM 密钥交换**: 基于 SM2 椭圆曲线的密钥封装
- **ML-KEM-768 混合交换**: NIST 后量子算法与传统算法混合
- **IKE_INTERMEDIATE 扩展**: 证书延迟分发机制
- **Early Gating**: DoS 防护机制
- **5-RTT 握手流程**: 安全性与性能的平衡

### 技术栈

- **strongSwan 6.0.4**: 开源 IPsec 实现
- **GmSSL 3.1.3**: 国密算法库 (SM2/SM3/SM4)
- **ML-KEM**: Module-Lattice-Based Key Encapsulation Mechanism

---

## 目录结构

```
PQGM-IPSec/
├── docs/                      # 项目文档
│   ├── 参考文档/              # 论文、草案等
│   ├── SM2_INTEGRATION_PLAN.md # SM2集成方案
│   └── chapter5_update.md     # 第五章实验数据
├── strongswan/                # strongSwan 源码（含gmalg插件）
│   └── src/libstrongswan/plugins/gmalg/
├── GmSSL/                     # GmSSL 国密库
├── tests/                     # 测试脚本和配置
│   ├── initiator/             # 发起方配置
│   ├── responder/             # 响应方配置
│   ├── results/               # 测试结果
│   └── *.sh                   # 自动化脚本
└── README.md                  # 本文件
```

---

## 快速开始

### 1. 环境要求

- Ubuntu 22.04 LTS (推荐)
- 两台虚拟机进行通信测试
- GCC 编译器
- CMake 3.10+

### 2. 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake libgmp-dev \
    libssl-dev pkg-config flex bison python3-pip
```

### 3. 编译 GmSSL

```bash
cd GmSSL
mkdir build && cd build
cmake ..
make
sudo make install
```

### 4. 编译 strongSwan（含 gmalg 插件）

```bash
cd strongswan
./autogen.sh
./configure --enable-gmalg --enable-swanctl \
    --with-gmssl=/usr/local
make
sudo make install
```

### 5. 运行测试

参见 [tests/README.md](tests/README.md) 获取详细测试指南。

---

## 实验结果

### 握手时延对比

| 配置 | 平均时延 | 增加量 | 增加比例 |
|------|---------|--------|----------|
| 基线 (x25519) | 48 ms | - | - |
| 混合 (x25519 + ML-KEM-768) | 52 ms | +4 ms | +8.3% |
| PQ-GM-IKEv2 (计划中) | TBD | TBD | TBD |

### 通信开销对比

| 配置 | 总报文大小 | 增加量 | 增加比例 |
|------|-----------|--------|----------|
| 基线 (2-RTT) | 4238 字节 | - | - |
| 混合 (4-RTT) | 9534 字节 | +5296 字节 | +125% |
| PQ-GM-IKEv2 (5-RTT) | TBD | TBD | TBD |

---

## 开发路线图

- [x] Phase 1: 环境搭建与基线测试
- [x] Phase 2: ML-KEM 混合密钥交换测试
- [x] Phase 3: GmSSL 安装与验证
- [x] Phase 4: gmalg 插件框架创建
- [ ] Phase 5: SM2 签名算法实现
- [ ] Phase 6: SM2-KEM 密钥交换实现
- [ ] Phase 7: 双证书机制集成
- [ ] Phase 8: IKE_INTERMEDIATE 扩展
- [ ] Phase 9: 端到端测试验证

---

## 参考文献

1. **RFC 7296**: Internet Key Exchange Protocol Version 2 (IKEv2)
2. **RFC 9370**: Multiple Key Exchanges in IKEv2
3. **RFC 9528**: Algorithm Identifiers for ML-KEM in IKEv2
4. **GM/T 0002-0004**: 中国国密算法标准
5. **draft-pqc-gm-ikev2**: PQ-GM-IKEv2 协议草案

---

## 许可证

本项目仅供学术研究使用。strongSwan 遵循 GPL-2.0 许可证，GmSSL 遵循其自身许可证。

---

## 联系方式

- 项目主页: [GitHub Repository](https://github.com/yourusername/PQGM-IPSec)
- 问题反馈: [Issues](https://github.com/yourusername/PQGM-IPSec/issues)

---

**Copyright (C) 2025 PQGM-IKEv2 Project**
