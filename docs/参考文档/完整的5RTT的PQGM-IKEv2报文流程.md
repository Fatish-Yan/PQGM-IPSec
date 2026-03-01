## 完整的5RTT的PQGM-IKEv2报文流程（固定 3 个 IKE_INTERMEDIATE）

### 0.准备动作
- 发起方I准备自己的SM2签名证书、SM2加密证书、AUTH证书；完成IKE配置（包括5RTT提案、三个证书的本地路径等）
- 响应方R准备自己的SM2签名证书、SM2加密证书、AUTH证书；完成IKE配置（包括5RTT提案、三个证书的本地路径等）

I发起IKE协商：
### 1 IKE_SA_INIT
- 双方按照RFC9370交换完成初始交换，协商出SA与DH密钥SK

### 2 IKE_INTERMEDIATE #1：双证书分发（不更新密钥）
- 发送SM2双证书：I -> R：SK { CERT(SignCert_i), CERT(EncCert_i), [CERTREQ] }
- 发送SM2双证书：R -> I：SK { CERT(SignCert_r), CERT(EncCert_r) }

交互完成后（即受到对方的双证书）:I从EncCert_r中提取R的 加密公钥,R从EncCert_i中提取I的加密公钥。存储并流转给SM2-KEM使用。
约束：
- 不携带 KE payload；
- 不触发 RFC 9370 密钥更新（沿用 IKE_SA_INIT 派生的 SK_*）。

### 3 IKE_INTERMEDIATE #2：ADDKE1 = SM2-KEM（MUST）
一往返“双向封装”
- I：
  - 生成 r_i（32B）
  - 使用R的加密公钥封装/加密得到 ct_i
  - 发送：SK { ct_i }
- R：
  - 生成 r_r（32B）
  - 使用I的加密公钥封装/加密得到 ct_r
  - 发送：SK { ct_r }

交互完成后（即受到对方的SM2-KEM密文）:I用自己的加密私钥从ct_r解密得到r_r,R用自己的加密私钥从ct_i解密得到r_i。拼接得到共享秘密。
- ss_sm2 = r_i || r_r，长度为 64B  
- 拼接顺序固定：先 r_i 后 r_r
完成后按RFC9370更新密钥，得到 SKEYSEED(1)、SK_*(1)。

### 4.4 IKE_INTERMEDIATE #3：ADDKE2 = ML-KEM-768（MUST）
此步骤是标准的ML-KEM密钥封装，strongswan已经实现，直接调用即可,但注意此时加密材料是SM2-KEM步骤更新过后的新密钥材料
- I -> R：SK { KEi(2)[group=ml-kem-768] }
- R -> I：SK { KEr(2)[group=ml-kem-768] }
- mlkem_ss（由 ML-KEM 解封装得到）

完成后按RFC9370更新密钥，得到 SKEYSEED(2)、SK_*(2)。

### 4.5 IKE_AUTH：（MUST）
- I -> R：SK { IDi, CERT(AUTH证书), AUTH(签名), SA, TSi, TSr }
- R -> I：SK { IDr, CERT(AUTH证书), AUTH(签名), SA, TSi, TSr }

要求：
- 必须将所有 intermediate 的关键内容纳入 AUTH 绑定（按 RFC 9242 的 IntAuth 思路），确保任意篡改导致认证失败；
---

