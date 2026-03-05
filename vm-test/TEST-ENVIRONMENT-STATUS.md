# VM测试环境状态

## 当前机器 (VM2 - Responder)

**状态**: 已配置完成 ✅

**网络配置**:
- IP: 192.168.172.132
- 主机名: ipsec-virtual-machine
- 接口: ens33

**strongSwan状态**:
- charon: 运行中 ✅
- 连接配置: 已加载
  - pqgm-5rtt-mldsa ✅
  - pqgm-5rtt-gm-symm ✅

**SM2-KEM性能优化**: 已启用 ✅
- 私钥预加载: 成功
- 预期RTT3延迟: ~1.4ms (vs 31.5ms未优化)

**证书**:
- responder_hybrid_cert.pem (ML-DSA混合证书) ✅
- mldsa_ca.pem (CA证书) ✅

**私钥**:
- responder_mldsa_key.bin (ML-DSA私钥) ✅
- encKey.pem (SM2加密私钥) ✅

## 克隆机器 (VM1 - Initiator)

**状态**: 待克隆和配置

**需要配置**:
- IP: 192.168.172.134
- 主机名: initiator.pqgm.test
- swanctl配置: /home/ipsec/PQGM-IPSec/vm-test/initiator/swanctl.conf

## 测试文件位置

| 文件 | 路径 |
|------|------|
| 配置脚本 | /home/ipsec/PQGM-IPSec/vm-test/scripts/ |
| 测试脚本 | /home/ipsec/PQGM-IPSec/vm-test/scripts/run_test.sh |
| PCAP捕获 | /home/ipsec/PQGM-IPSec/vm-test/scripts/start_capture.sh |
| 日志收集 | /home/ipsec/PQGM-IPSec/vm-test/scripts/collect_logs.sh |
| 测试结果 | /home/ipsec/PQGM-IPSec/vm-test/results/ |

## 下一步操作

### 1. 克隆VM (在VMware中操作)
```
1. 关闭当前VM
2. VMware → 右键当前VM → 管理 → 克隆
3. 克隆类型: 完整克隆
4. 命名: PQGM-IPSec-Initiator
5. 生成新MAC地址: 是
```

### 2. 配置Initiator (在克隆后的VM上)
```bash
# 启动克隆后的VM，登录后执行
sudo /home/ipsec/PQGM-IPSec/vm-test/scripts/setup_initiator.sh

# 验证网络
ping 192.168.172.132

# 启动charon
sudo bash -c 'export LD_PRELOAD=/usr/local/lib/libgmssl.so.3; /usr/local/libexec/ipsec/charon --debug-ike 2 &'

# 加载配置
sudo swanctl --load-all
```

### 3. 测试连接
```bash
# 在Initiator上执行
sudo swanctl --initiate --child net --ike pqgm-5rtt-mldsa
```

## 证书和私钥清单

### Initiator (VM1 - 192.168.172.134)

| 组件 | 文件 | 状态 | 说明 |
|------|------|------|------|
| ML-DSA混合证书 | initiator_hybrid_cert.pem | ✅ | CN=initiator.pqgm.test, issuer=PQGM-MLDSA-CA |
| ML-DSA私钥 | initiator_mldsa_key.bin | ✅ | 4032 bytes |
| SM2签名证书 | sign_cert.pem | ✅ | CN=initiator.pqgm-sign |
| SM2签名私钥 | sign_key.pem | ✅ | 加密PEM |
| SM2加密证书 | enc_cert.pem | ✅ | CN=initiator.pqgm-enc |
| SM2加密私钥 | enc_key.pem | ✅ | 加密PEM (密码: PQGM2026) |
| SM2 CA | caCert.pem | ✅ | CN=PQGM-SM2-CA |
| ML-DSA CA | mldsa_ca.pem | ✅ | CN=PQGM-MLDSA-CA |

### Responder (VM2 - 192.168.172.132)

| 组件 | 文件 | 状态 | 说明 |
|------|------|------|------|
| ML-DSA混合证书 | responder_hybrid_cert.pem | ✅ | CN=responder.pqgm.test, issuer=PQGM-MLDSA-CA |
| ML-DSA私钥 | responder_mldsa_key.bin | ✅ | 4032 bytes |
| SM2签名证书 | sign_cert.pem | ✅ | CN=responder.pqgm-sign |
| SM2签名私钥 | sign_key.pem | ✅ | 加密PEM |
| SM2加密证书 | enc_cert.pem | ✅ | CN=responder.pqgm-enc |
| SM2加密私钥 | enc_key.pem | ✅ | 加密PEM (密码: PQGM2026) |
| SM2 CA | caCert.pem | ✅ | CN=PQGM-SM2-CA |
| ML-DSA CA | mldsa_ca.pem | ✅ | CN=PQGM-MLDSA-CA |

### 证书验证说明

> **重要**: OpenSSL `verify` 命令报错是**正常的**，不影响实际使用！

- **ML-DSA 证书**: OpenSSL 不支持 ML-DSA 签名算法，由 strongSwan `mldsa` 插件处理
- **SM2 证书**: OpenSSL verify 不支持 SM2 签名算法，由 strongSwan `gmalg` 插件处理
- 证书链验证在 strongSwan 内部完成，不依赖 OpenSSL

### 证书源文件位置

```
/home/ipsec/PQGM-IPSec/docker/initiator/certs/
├── x509/           # 证书文件
├── private/        # 私钥文件
├── x509ca/         # CA证书
└── mldsa/          # ML-DSA相关证书

/home/ipsec/PQGM-IPSec/docker/responder/certs/
├── x509/
├── private/
├── x509ca/
└── mldsa/
```

## 已知问题

1. ~~**SM2-KEM私钥预加载失败**~~ ✅ 已修复
   - 原因: gmalg.conf缺少`enc_key_secret`配置
   - 修复: 添加 `enc_key_secret = PQGM2026` 到 gmalg.conf

2. **部分证书解析失败**
   - 原因: x509目录中有一些非证书文件
   - 影响: 不影响核心功能

## 测试命令速查

```bash
# 检查charon状态
pgrep -a charon

# 重新加载配置
sudo swanctl --load-all

# 列出连接
sudo swanctl --list-conns

# 列出证书
sudo swanctl --list-certs

# 发起连接
sudo swanctl --initiate --child net --ike pqgm-5rtt-mldsa

# 查看SA
sudo swanctl --list-sas

# 终止连接
sudo swanctl --terminate --ike all

# 查看日志
tail -f /var/log/charon.log
```
