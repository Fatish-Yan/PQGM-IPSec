# PQGM-IKEv2: 抗量子IPSec协议设计与实现

## 项目简介

本项目实现了 **PQ-GM-IKEv2 协议**，这是一种结合了**后量子密码学**和**国密算法**的增强型 IKEv2 协议。该协议为 IPsec VPN 提供了抗量子计算攻击的安全保护。

### 核心特性

- **SM2 双证书机制**: 签名证书 + 加密证书分离
- **SM2-KEM 密钥交换**: 基于 SM2 椭圆曲线的密钥封装
- **ML-KEM-768 混合交换**: NIST 后量子算法与传统算法混合
- **ML-DSA-65 认证**: 后量子数字签名
- **IKE_INTERMEDIATE 扩展**: 证书延迟分发机制
- **Early Gating**: DoS 防护机制
- **5-RTT 握手流程**: 安全性与性能的平衡

### 技术栈

- **strongSwan 6.0.4**: 开源 IPsec 实现
- **GmSSL 3.1.3**: 国密算法库 (SM2/SM3/SM4)
- **liboqs 0.10.0**: 后量子算法库 (ML-KEM/ML-DSA)
- **ML-KEM-768**: Module-Lattice-Based Key Encapsulation
- **ML-DSA-65**: Module-Lattice-Based Digital Signature

---

## 目录结构

```
PQGM-IPSec/
├── docs/                      # 项目文档
│   ├── 参考文档/              # 论文、草案等
│   ├── TEST-ENVIRONMENT.md    # Docker 测试环境说明
│   ├── FIXES-RECORD.md        # 修复记录
│   ├── BUG-RECORD.md          # BUG 记录
│   └── ML-DSA-5RTT-THESIS-DATA.md # 论文实验数据
├── docker/                    # Docker 测试环境
│   ├── initiator/             # 发起方配置
│   ├── responder/             # 响应方配置
│   └── docker-compose.yml     # Docker Compose 配置
├── strongswan/                # strongSwan 源码（含 gmalg/mldsa 插件）
│   └── src/libstrongswan/plugins/
│       ├── gmalg/              # 国密算法插件
│       └── mldsa/              # ML-DSA 签名插件
├── GmSSL/                     # GmSSL 国密库
└── README.md                  # 本文件
```

---

## 快速开始

### 1. 环境要求

- Ubuntu 22.04 LTS (推荐)
- Docker & Docker Compose (用于测试环境)
- GCC 编译器
- CMake 3.10+

### 2. 编译依赖

```bash
# 安装编译依赖
sudo apt update
sudo apt install -y build-essential cmake libgmp-dev \
    libssl-dev pkg-config flex bison autoconf automake libtool

# 编译 GmSSL
cd GmSSL
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
cd ../..

# 编译 strongSwan (含 gmalg + mldsa 插件)
cd strongswan
./autogen.sh
./configure --enable-gmalg --enable-mldsa --enable-swanctl \
    --with-gmssl=/usr/local
make -j$(nproc)
sudo make install
cd ..
```

### 3. 配置插件 (重要!)

**配置文件**: `docker/initiator/config/strongswan.conf`

```conf
charon {
    load_modular = yes
    plugins {
        gmalg {
            load = yes
            # SM2 双证书配置 (文件名，放在 /usr/local/etc/swanctl/x509/)
            sign_cert = signCert.pem
            enc_cert = encCert.pem
            # SM2 加密私钥 (放在 /usr/local/etc/swanctl/private/)
            enc_key = enc_key.pem
            # 私钥密码
            enc_key_secret = PQGM2026
        }
        mldsa {
            load = yes
        }
    }
}
```

**配置说明**:

| 配置键 | 说明 | 默认值 |
|--------|------|--------|
| `sign_cert` | SM2 签名证书文件名 | - |
| `enc_cert` | SM2 加密证书文件名 | - |
| `enc_key` | SM2 加密私钥文件名 | `enc_key.pem` |
| `enc_key_secret` | 私钥密码 | - |

### 4. 运行 Docker 测试

```bash
# 启动容器
cd docker
sudo docker-compose up -d

# 发起连接
sudo docker exec pqgm-initiator swanctl --load-all
sudo docker exec pqgm-responder swanctl --load-all
sudo docker exec pqgm-initiator swanctl --initiate --child net

# 查看日志
sudo docker logs pqgm-initiator
```

---

## 实验结果

### 5-RTT 握手时延

| RTT | 阶段 | 耗时 |
|-----|------|------|
| 1 | IKE_SA_INIT | ~0.8 ms |
| 2 | IKE_INTERMEDIATE #0 (双证书) | ~0.6 ms |
| 3 | IKE_INTERMEDIATE #1 (SM2-KEM) | ~35 ms |
| 4 | IKE_INTERMEDIATE #2 (ML-KEM-768) | ~1.8 ms |
| 5 | IKE_AUTH (ML-DSA-65) | ~4.0 ms |
| **总计** | | **~42 ms** |

### 通信开销

| 配置 | 总报文大小 | RTT 数 |
|------|-----------|--------|
| 标准 IKEv2 (2-RTT) | ~2000 字节 | 2 |
| PQ-GM-IKEv2 (5-RTT) | ~19905 字节 | 5 |

### 安全强度

| 算法 | 安全级别 | 抗量子 |
|------|---------|--------|
| x25519 | 128 位 (古典) | ❌ |
| SM2-KEM | 256 位 (古典) | ❌ |
| ML-KEM-768 | 192 位 (经典) / 128 位 (量子) | ✅ |
| ML-DSA-65 | NIST Level 3 | ✅ |

---

## 开发路线图

- [x] Phase 1: 环境搭建与基线测试
- [x] Phase 2: ML-KEM 混合密钥交换测试
- [x] Phase 3: GmSSL 安装与验证
- [x] Phase 4: gmalg 插件框架创建
- [x] Phase 5: SM2 签名算法实现
- [x] Phase 6: SM2-KEM 密钥交换实现
- [x] Phase 7: 双证书机制集成
- [x] Phase 8: IKE_INTERMEDIATE 扩展
- [x] Phase 9: ML-DSA-65 认证实现
- [x] Phase 10: 5-RTT 端到端测试验证
- [x] Phase 11: 插件配置化 (移除硬编码)

---

## 参考文献

1. **RFC 7296**: Internet Key Exchange Protocol Version 2 (IKEv2)
2. **RFC 9242**: Intermediate Exchange in the Internet Key Exchange Protocol Version 2 (IKEv2)
3. **RFC 9370**: Multiple Key Exchanges in IKEv2
4. **FIPS 203**: Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM)
5. **FIPS 204**: Module-Lattice-Based Digital Signature Standard (ML-DSA)
6. **GM/T 0002-0004**: 中国国密算法标准
7. **draft-pqc-gm-ikev2**: PQ-GM-IKEv2 协议草案

---

## 许可证

本项目仅供学术研究使用。strongSwan 遵循 GPL-2.0 许可证，GmSSL 遵循其自身许可证。

---

## 联系方式

- 项目主页: [GitHub Repository](https://github.com/yourusername/PQGM-IPSec)
- 问题反馈: [Issues](https://github.com/yourusername/PQGM-IPSec/issues)

---

**Copyright (C) 2025-2026 PQGM-IKEv2 Project**
