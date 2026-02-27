# M5 本地回环测试计划

> 创建时间: 2026-02-27
> 目的: 验证 PQ-GM-IKEv2 完整 5 RTT 协议流程

---

## 1. 测试目标

验证 PQ-GM-IKEv2 的完整协议流程：

```
RTT 1: IKE_SA_INIT (协商 x25519 + ML-KEM + SM2-KEM)
RTT 2: IKE_INTERMEDIATE #0 (双证书分发)
RTT 3: IKE_INTERMEDIATE #1 (SM2-KEM 密钥交换)
RTT 4: IKE_INTERMEDIATE #2 (ML-KEM-768 密钥交换)
RTT 5: IKE_AUTH (SM2 签名认证)
```

---

## 2. 测试环境

### 2.1 本地回环配置

由于是单机测试，需要修改配置使用 127.0.0.1：

**Initiator 配置**:
```conf
local_addrs = 127.0.0.1
remote_addrs = 127.0.0.1
```

### 2.2 前置条件检查

- [x] strongSwan 6.0.4 已编译安装 (04:09)
- [x] ike_cert_post.c 已修改 (message_id == 1)
- [x] gmalg 插件已编译
- [x] ml 插件可用
- [ ] 证书文件已部署到 /etc/swanctl/
- [ ] swanctl.conf 已部署到 /etc/swanctl/

---

## 3. 测试步骤

### Task 1: 准备本地回环配置

**Step 1: 创建本地测试目录**

```bash
sudo mkdir -p /etc/swanctl/x509
sudo mkdir -p /etc/swanctl/x509ca
sudo mkdir -p /etc/swanctl/private
sudo mkdir -p /etc/swanctl/pubkey
```

**Step 2: 复制证书文件**

```bash
# CA 证书
sudo cp /home/ipsec/PQGM-IPSec/certs/ca/ca_sm2_cert.pem /etc/swanctl/x509ca/

# 本地证书 (使用 initiator 的)
sudo cp /home/ipsec/PQGM-IPSec/certs/initiator/sign_cert.pem /etc/swanctl/x509/
sudo cp /home/ipsec/PQGM-IPSec/certs/initiator/enc_cert.pem /etc/swanctl/x509/

# 私钥
sudo cp /home/ipsec/PQGM-IPSec/certs/initiator/sign_key.pem /etc/swanctl/private/
sudo cp /home/ipsec/PQGM-IPSec/certs/initiator/enc_key.pem /etc/swanctl/private/
```

**Step 3: 创建本地回环 swanctl.conf**

```bash
sudo tee /etc/swanctl/swanctl.conf << 'EOF'
# PQ-GM-IKEv2 本地回环测试配置
# Triple Key Exchange: x25519 + ML-KEM-768 + SM2-KEM

connections {
    pqgm-loopback {
        version = 2
        local_addrs = 127.0.0.1
        remote_addrs = 127.0.0.1

        # IKE SA proposals with triple key exchange
        proposals = aes256-sha384-x25519-ke1_mlkem768-ke2_sm2kem

        local {
            auth = pubkey
            certs = sign_cert.pem
            id = local.pqgm.test
        }

        remote {
            auth = pubkey
            id = local.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.1.0.0/16
                remote_ts = 10.2.0.0/16

                esp_proposals = aes256gcm256-x25519-ke1_mlkem768-ke2_sm2kem

                updown = /usr/local/libexec/ipsec/_updown iptables
            }
        }
    }
}

secrets {
    sm2-sign {
        file = sign_key.pem
        secret = "PQGM2026"
    }
    sm2-enc {
        file = enc_key.pem
        secret = "PQGM2026"
    }
}
EOF
```

**Step 4: 验证配置语法**

```bash
sudo swanctl --load-all --debug
```

Expected: 配置加载成功，无语法错误

---

### Task 2: 启动 strongSwan 并捕获日志

**Step 1: 停止现有进程**

```bash
sudo pkill charon 2>/dev/null || true
sudo ip xfrm state flush 2>/dev/null || true
sudo ip xfrm policy flush 2>/dev/null || true
```

**Step 2: 启动 charon 并开启详细日志**

```bash
# 终端 1: 启动 charon
sudo charon --debug-all 2>&1 | tee /tmp/pqgm_charon.log
```

**Step 3: 等待 charon 启动**

```bash
# 终端 2: 等待并检查
sleep 3
sudo swanctl --list-sas
```

Expected: charon 启动成功，无错误

---

### Task 3: 触发 IKE 协商

**Step 1: 发起连接**

```bash
# 终端 2: 发起 IKE 协商
sudo swanctl --initiate --child ipsec 2>&1 | tee /tmp/pqgm_initiate.log
```

**Step 2: 观察日志输出**

在终端 1 观察 charon 日志，查找关键信息：

1. **IKE_SA_INIT**:
   ```
   IKE_SA_INIT request received
   negotiating proposals: aes256-sha384-x25519-ke1_mlkem768-ke2_sm2kem
   ```

2. **IKE_INTERMEDIATE #0** (证书分发):
   ```
   PQ-GM-IKEv2: will send certificates in IKE_INTERMEDIATE #0 (mid=1)
   PQ-GM-IKEv2: sending SignCert in IKE_INTERMEDIATE
   PQ-GM-IKEv2: sending EncCert in IKE_INTERMEDIATE
   ```

3. **IKE_INTERMEDIATE #1** (SM2-KEM):
   ```
   ADDKE1 execution
   SM2-KEM key exchange
   ```

4. **IKE_INTERMEDIATE #2** (ML-KEM):
   ```
   ADDKE2 execution
   ML-KEM-768 key exchange
   ```

5. **IKE_AUTH**:
   ```
   IKE_AUTH request
   authentication successful
   ```

---

### Task 4: 验证结果

**Step 1: 检查 IKE SA 状态**

```bash
sudo swanctl --list-sas
```

Expected: 显示已建立的 IKE SA

**Step 2: 检查 IPsec SA 状态**

```bash
sudo ip xfrm state
sudo ip xfrm policy
```

Expected: 显示 IPsec SA 和策略

**Step 3: 检查日志中的 RTT 数量**

```bash
grep -c "IKE_INTERMEDIATE\|IKE_AUTH" /tmp/pqgm_charon.log
```

Expected: 多个 INTERMEDIATE + AUTH 消息

---

### Task 5: 性能测量

**Step 1: 测量协商时延**

```bash
# 记录开始时间
START=$(date +%s%N)

# 发起协商
sudo swanctl --initiate --child ipsec

# 记录结束时间
END=$(date +%s%N)

# 计算时延
echo "Total time: $(( (END - START) / 1000000 )) ms"
```

**Step 2: 多次测量取平均**

```bash
for i in {1..5}; do
    sudo ip xfrm state flush
    sudo ip xfrm policy flush
    sudo pkill charon; sleep 2
    sudo charon --debug-all &
    sleep 3
    START=$(date +%s%N)
    sudo swanctl --initiate --child ipsec
    END=$(date +%s%N)
    echo "Run $i: $(( (END - START) / 1000000 )) ms"
done
```

---

## 4. 预期结果

### 4.1 功能验证

| 检查项 | 预期结果 |
|--------|----------|
| IKE_SA_INIT | 成功协商三重 KE |
| IKE_INTERMEDIATE #0 | 双证书分发成功 |
| IKE_INTERMEDIATE #1 | SM2-KEM 执行 |
| IKE_INTERMEDIATE #2 | ML-KEM-768 执行 |
| IKE_AUTH | SM2 签名认证成功 |
| IPsec SA | 成功建立 |

### 4.2 性能预期

| 指标 | 传统 IKEv2 | PQ-GM-IKEv2 | 增量 |
|------|-----------|-------------|------|
| RTT | 2 | 5 | +3 |
| 时延 | ~48ms | ~80ms | +32ms |
| 密文大小 | 32B | 141B + 1184B | +1293B |

---

## 5. 故障排查

### 问题 1: 提案协商失败

**症状**: `no acceptable proposal found`

**排查**:
```bash
# 检查插件加载
sudo swanctl --stats

# 检查 gmalg 插件
ls /usr/local/lib/ipsec/plugins/libstrongswan-gmalg.so
```

### 问题 2: 证书分发未触发

**症状**: 无 "PQ-GM-IKEv2" 日志

**排查**:
```bash
# 检查证书是否正确加载
sudo swanctl --list-certs

# 检查证书策略
grep "cert_policy" /etc/swanctl/swanctl.conf
```

### 问题 3: ADDKE 未执行

**症状**: 只有 IKE_SA_INIT 和 IKE_AUTH

**排查**:
```bash
# 检查提案中的 ke1_ 和 ke2_
grep "ke1_\|ke2_" /etc/swanctl/swanctl.conf

# 检查 ml 插件
ls /usr/local/lib/ipsec/plugins/libstrongswan-ml.so
```

---

## 6. 测试完成标准

- [ ] IKE_SA_INIT 成功协商三重 KE
- [ ] IKE_INTERMEDIATE #0 双证书分发日志出现
- [ ] IKE_INTERMEDIATE #1/#2 ADDKE 执行日志出现
- [ ] IKE_AUTH 认证成功
- [ ] IPsec SA 成功建立
- [ ] 时延数据记录

---

## 7. 下一步

测试通过后：
1. 记录性能数据用于论文
2. 更新 MODULES.md 标记 M5 完成
3. 进行双机部署测试
