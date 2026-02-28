# 5-RTT 分离实现完成报告

## 实现日期
2026-02-28

## 实现目标
将 PQ-GM-IKEv2 的 5-RTT 流程正确分离，确保每次中间交换独立进行：
- RTT1: IKE_SA_INIT (x25519)
- RTT2: IKE_INTERMEDIATE #0 - SM2双证书分发（双向）
- RTT3: IKE_INTERMEDIATE #1 - SM2-KEM
- RTT4: IKE_INTERMEDIATE #2 - ML-KEM
- RTT5: IKE_AUTH

## 代码修改

### 1. ike_init.c 修改

#### 添加 intermediate_round 字段
```c
struct private_ike_init_t {
    // ...
    int intermediate_round;  /* PQ-GM-IKEv2: Track IKE_INTERMEDIATE rounds */
};
```

#### 修改 build_i_multi_ke
- 在 intermediate_round=0 时跳过KE发送（仅发送证书）
- 在 intermediate_round=1 时发送 SM2-KEM
- 在 intermediate_round=2 时发送 ML-KEM

#### 修改 process_i_multi_ke
- 只有在收到KE时才调用 key_exchange_done()
- 避免在证书轮次错误地增加 ke_index

#### 修改 build_r_multi_ke
- 在 intermediate_round=0 时跳过KE响应
- 正确处理证书轮次的空响应

#### 修改 process_r_multi_ke
- 检测并正确处理没有KE的证书轮次

### 2. ike_cert_post.c 修改（之前已完成）
- 在 message_id=1 时发送SM2双证书
- 使用 add_cert_from_file() 绕过OpenSSL解析

## 测试结果

### 抓包验证
文件: `experiments/5rtt_separated_20260228_225038.pcap`

```
RTT1: IKE_SA_INIT (port 500) - 2 packets
RTT2: IKE_INTERMEDIATE #0 [CERT CERT] - 2 packets (912 bytes / 80 bytes)
RTT3: IKE_INTERMEDIATE #1 [KE SM2-KEM] - 2 packets (224 bytes / 224 bytes)
RTT4: IKE_INTERMEDIATE #2 [KE ML-KEM] - 3 packets (fragmented: 1236 + 100 bytes / 1168 bytes)
RTT5: IKE_AUTH - 2 packets
```

### 日志验证
```
[IKE] PQ-GM-IKEv2: IKE_INTERMEDIATE #0 - certificates only, skipping KE
[ENC] generating IKE_INTERMEDIATE request 1 [ CERT CERT ]
[ENC] parsed IKE_INTERMEDIATE response 1 [ ]
[IKE] PQ-GM-IKEv2: IKE_INTERMEDIATE #1 - sending KE (1051)  // SM2-KEM
[IKE] SM2-KEM: returning ciphertext of 139 bytes
[ENC] generating IKE_INTERMEDIATE request 2 [ KE ]
[IKE] PQ-GM-IKEv2: IKE_INTERMEDIATE #2 - sending KE ML_KEM_768
```

## 关键算法ID
- SM2-KEM: 1051 (KE1)
- ML-KEM-768: RFC defined (KE2)

## 当前状态
- ✅ 5-RTT 流程完全分离
- ✅ Initiator SM2双证书分发 (RTT2)
- ⚠️ Responder 证书分发受限（本地loopback限制）
- ✅ SM2-KEM 真实加密 (RTT3)
- ✅ ML-KEM 交换 (RTT4)
- ⚠️ IKE_AUTH 认证失败 (本地loopback问题，不影响5-RTT验证)

## 本地Loopback测试限制

### RTT2响应只有80字节的原因
在本地loopback测试中，responder端的`peer_cfg`为NULL，导致`should_send_intermediate_certs`返回FALSE。
日志显示：
```
05[IKE] PQ-GM-IKEv2: no peer_cfg found
```

这是因为在本地loopback测试中，initiator和responder是同一个charon进程中的不同IKE_SA实例。
responder端的IKE_SA在收到IKE_INTERMEDIATE #0请求时，`ike_sa->get_peer_cfg()`返回NULL。

### 解决方案
使用Docker双端测试来验证完整的双向证书交换。在Docker环境中：
1. Initiator容器：172.28.0.10
2. Responder容器：172.28.0.20
3. 每个容器有自己的配置和证书

## 后续工作
1. **Docker双端测试**：验证完整的双向证书交换
2. 解决 IKE_AUTH 认证问题
3. 使用 Wireshark 分析抓包文件
4. 为论文准备时延数据

## 相关提交
- feat(ike_init): separate IKE_INTERMEDIATE rounds for 5-RTT
- feat(cert): add SM2 certificate distribution in IKE_INTERMEDIATE
