## draft-<yourname>-pqc-gm-ikev2-00

```
Internet-Draft                                               <yyy>
Intended status: Experimental                                <xdu>
Expires: 9 July 2026                                         7 January 2026

        抗量子与国密融合的 IKEv2 扩展设计
        基于 IKE_INTERMEDIATE 与多重密钥交换的双证书密钥交换机制
                 draft-<yourname>-pqc-gm-ikev2-00
```

### 摘要（Abstract）

本文档提出一种面向量子计算威胁的 IPsec/IKEv2 改造方案：在保持 IKEv2 主流程（IKE_SA_INIT / IKE_AUTH）不变的前提下，利用 **IKE_INTERMEDIATE** 交换在 IKE_SA_INIT 与 IKE_AUTH 之间插入一个或多个中间交换，用于（1）协商并执行**多重密钥交换**（Multiple Key Exchanges），实现经典 DH 与后量子 KEM 的混合密钥建立；（2）引入体现中国密码学工程特色的**SM2 双证书机制**（签名证书/加密证书分离），在中间交换阶段完成基于加密证书的“国密风格”密钥交换，并将其与后量子共享秘密共同注入 IKEv2 密钥派生过程。该方案基于 IKEv2 的 IKE_INTERMEDIATE（RFC 9242）与多重密钥交换框架（RFC 9370），并建议以 strongSwan 6.0+ 为实现与评估平台。此外，本文档补充 IKE_AUTH 阶段的抗量子认证：支持使用后量子签名证书（例如 ML-DSA/SLH-DSA）完成 AUTH 认证，并通过 IntAuth 将全部 IKE_INTERMEDIATE 交换绑定到身份。 ([IETF Datatracker][1])

---

## 本备忘录状态（Status of This Memo）

本文档为 Internet-Draft 风格的实验性说明文本，用于毕业设计的协议设计与工程验证。本文档内容可能随实现、测试与安全分析结论更新。

---

## 目录（Table of Contents）

1. 引言
2. 术语与约定
3. 设计目标与威胁模型
4. 协议总览
5. 协商与报文流程
6. 双证书机制与“国密风格”密钥交换（SM2-KEM）
7. 后量子 KEM（ML-KEM）与混合注入策略
8. 密钥派生与密钥更新
9. 证书与认证（含抗量子证书的加入位置）
10. strongSwan 实现建议（概要）
11. 安全性考虑
12. IANA 考虑
13. 参考文献

---

## 1. 引言（Introduction）

量子计算在可预见未来可能削弱基于离散对数/椭圆曲线等传统公钥密码的安全性，带来“**先录制、后解密**”（Store-Now-Decrypt-Later, SNDL）风险，尤其威胁 VPN/IPsec 的长期机密性。为在兼容现网 IKEv2 的同时获得抗量子能力，业界常用**混合密钥交换**：同时执行经典 DH 与后量子 KEM，将两者共享秘密混合进入会话密钥派生，从而在“后量子算法被攻破”与“传统算法被量子攻破”两种不确定性下获得折中安全性。

IKEv2 已标准化 **IKE_INTERMEDIATE**（RFC 9242）用于在 IKE_SA_INIT 与 IKE_AUTH 之间插入中间交换，并标准化 **多重密钥交换**（RFC 9370）机制，使得最多可协商 7 轮额外密钥交换（ADDKE1..ADDKE7）。 ([IETF Datatracker][1])

此外，在中国商用密码工程体系中，**SM2 双证书**（签名证书与加密证书分离）是常见实践：签名证书用于身份认证与不可否认性；加密证书用于密钥封装/会话密钥保护。本文档将该“特色”以 IKEv2 扩展形式落地到中间交换阶段，并与后量子密钥建立结合。

---

## 2. 术语与约定（Terminology and Conventions）

本文档中的 “MUST / SHOULD / MAY …” 按 BCP 14（RFC 2119、RFC 8174）解释。

缩写：

* **IKEv2**：Internet Key Exchange Version 2（RFC 7296）
* **IKE_INTERMEDIATE**：IKEv2 中间交换（RFC 9242） ([IETF Datatracker][1])
* **ADDKE1..7**：额外密钥交换 Transform Type（RFC 9370；IANA Transform Type=6..12）
* **ML-KEM**：NIST 标准化后量子 KEM（FIPS 203） ([NIST计算机安全资源中心][2])
* **ML-DSA / SLH-DSA**：NIST 标准化后量子签名（FIPS 204 / FIPS 205） ([NIST计算机安全资源中心][3])
* **SM2/SM3/SM4**：中国商用密码算法（如 GB/T 35276-2017 等）
* **双证书**：SM2签名证书（SignCert）与加密证书（EncCert）分离
* **认证证书**：IKE_AUTH阶段用于认证和绑定的证书（AuthCert）,和签名证书区分,要求使用后量子算法

---

## 3. 设计目标与威胁模型（Goals and Threat Model）

### 3.1 设计目标

1. **抗量子机密性**：通过混合密钥交换抵御 SNDL。
2. **抗量子身份认证**：通过后量子认证证书保证身份认证的抗量子性，避免中间人攻击。
3. **国密特色**：引入 SM2 双证书与“基于加密证书的密钥交换/封装”流程。
4. **兼容与可实现**：尽量复用 RFC 9242/9370 的扩展点，减少新增 payload。
5. **可评估**：可在 strongSwan 6.0+ 上实现、抓包、压测与安全分析。 ([StrongSwan][4])

### 3.2 威胁模型（简述）

* 攻击者可被动监听并录制 IKE 与 ESP 流量（SNDL）。
* 攻击者可利用 PQ 大 payload 放大 DoS。
* 攻击者可破解传统签名算法，进行主动攻击。

---

## 4. 协议总览（Protocol Overview）

本文档方案基于 IKEv2 标准主流程：

* IKE_SA_INIT：协商算法 + 建立初始密钥材料
* （可选）IKE_INTERMEDIATE：若协商支持，则插入若干中间交换
* IKE_AUTH：身份认证 + 建立第一个 CHILD_SA

在 IKE_INTERMEDIATE 阶段，本方案引入两类动作：
A. **证书携带/双证书分发**（可不更新密钥，仅传数据）——RFC 9242 允许中间交换“传额外数据”。
B. **执行 ADDKE 轮次**（每轮携带 KEi(n)/KEr(n) 并触发密钥更新）——RFC 9370 规定每个额外密钥交换对应一个 IKE_INTERMEDIATE 往返，并顺序映射到协商的 ADDKE Transform Types。 ([IETF Datatracker][5])

---

## 5. 协商与报文流程（Negotiation and Message Flow）

### 5.1 IKE_SA_INIT 协商要点

发起方在 IKE_SA_INIT 请求中：

* MUST 在 SA 提案中包含 Transform Type 4（KE）以及可选的 ADDKE1..ADDKE7（Transform Type 6..12）。
* MUST 携带 `N(INTERMEDIATE_EXCHANGE_SUPPORTED)` 以表明支持 IKE_INTERMEDIATE。 ([IETF Datatracker][1])

其中，ML-KEM 已在 IANA 的 Transform Type 4（Key Exchange Method Transform IDs）里分配了 `ml-kem-512/768/1024`（35/36/37）。 ([StrongSwan][4])

### 5.2 推荐的初始建链时序（示例）

> 示例目标：
>
> * 主 KE：x25519（经典）
> * ADDKE1：sm2-kem（国密风格，私有 Transform ID）
> * ADDKE2：ml-kem-768（后量子）

```
IKE_SA_INIT:
  Initiator -> Responder: HDR, SAi1(KE=x25519, ADDKE1=sm2-kem, ADDKE2=ml-kem-768),
                          KEi, Ni, N(INTERMEDIATE_EXCHANGE_SUPPORTED)
  Responder -> Initiator: HDR, SAr1(...), KEr, Nr, [CERTREQ], N(INTERMEDIATE_EXCHANGE_SUPPORTED)

IKE_INTERMEDIATE #0  (双证书分发，仅传数据，可不带 KE):
  Initiator -> Responder: HDR, SK { CERT(SignCert_i), CERT(EncCert_i), [CERTREQ] }
  Responder -> Initiator: HDR, SK { CERT(SignCert_r), CERT(EncCert_r) }

IKE_INTERMEDIATE #1  (ADDKE1 = SM2-KEM):
  Initiator -> Responder: HDR, SK { KEi(2) [group=sm2-kem] }
  Responder -> Initiator: HDR, SK { KEr(2) [group=sm2-kem] }
  (双方更新密钥材料)


IKE_INTERMEDIATE #2  (ADDKE2 = ML-KEM-768):
  Initiator -> Responder: HDR, SK { KEi(1) [group=ml-kem-768] }
  Responder -> Initiator: HDR, SK { KEr(1) [group=ml-kem-768] }
  (双方更新密钥材料：见第 8 节)

IKE_AUTH:
  Initiator -> Responder: HDR, SK { IDi, [CERT(PQ)], AUTH, SA, TSi, TSr }
  Responder -> Initiator: HDR, SK { IDr, [CERT(PQ)], AUTH, SA, TSi, TSr }
```

说明：

* IKE_INTERMEDIATE #0 的“仅传证书”符合 RFC 9242：中间交换可选，用于传额外数据；若不发生新 KE，则 SK_* 不更新。
* IKE_INTERMEDIATE #1/#2 的 “第 n 个 KE payload” 必须匹配第 n 个协商成功的 ADDKE（按 RFC 9370 的顺序规则）。

---

## 6. 双证书机制与“国密风格”密钥交换（SM2-KEM）

### 6.1 设计意图

双证书（SignCert/EncCert）分离带来的工程价值：

* 签名证书仅用于IKE_INTERMEDIATE阶段的提前身份认证,防止Dos,和IKE_AUTH阶段的Auth_Cert区分；
* 加密证书用于“国密风格”的会话熵注入（类似 KEM/封装），体现国密体系的工程特征；
* 即使 SM2-KEM 本身不具备抗量子能力，也可与 ML-KEM 混合后仍获得抗量子机密性（取决于混合策略与至少一个秘密的安全性）。

### 6.2 SM2-KEM 作为 Key Exchange Method 的编码位置

* 本方案将 “SM2-KEM” 定义为 IKEv2 **Key Exchange Method Transform ID** 的一个私有值（Private Use）。IANA 指出 Transform Type 4 的 1024–65535 范围保留给私有用途。
* 在 IKE_INTERMEDIATE 的 KE payload 中：

  * `Group/Method` 字段填该私有 Transform ID（选择60001）；
  * `Key Exchange Data` 字段携带 SM2 公钥加密产生的密文/封装数据。

### 6.3 双向封装（one round trip）建议格式

为满足 RFC 9370 对“一个往返完成共享秘密导出”的约束（Key exchange method MUST take exactly one round trip），建议采用“双向封装”：

* Initiator 生成随机 `r_i`（建议 32B 或 48B），用 Responder 的 EncCert 公钥加密得到 `ct_i`，作为 `KEi(2)` 的 Data。
* Responder 解密得 `r_i`，再生成随机 `r_r`，用 Initiator 的 EncCert 公钥加密得到 `ct_r`，作为 `KEr(2)` 的 Data。
* 共享秘密定义为：`SK_sm2 = r_i || r_r`（或对其做一次 KDF/PRF 预处理）。

> 注：KE payload 位于 SK 加密保护之内，密文传输本身受 IKE 的完整性保护；最终在 IKE_AUTH 中通过 RFC 9242 的认证计算把所有中间交换绑定到身份上，降低中间人替换 EncCert 的风险。

---

## 7. 后量子 KEM（ML-KEM）与混合注入策略

### 7.1 ML-KEM 的选型与参数

* ML-KEM 来自 NIST FIPS 203（ML-KEM，原 Kyber）。 ([NIST计算机安全资源中心][2])
* IANA 已列出 `ml-kem-512/768/1024` 作为 IKEv2 Key Exchange Method Transform IDs（35/36/37）。
* strongSwan 6.0 文档已给出 ML-KEM 与 RFC 9370 的配置方式（`keX_` 前缀）。 ([StrongSwan][4])

工程上建议默认 `ml-kem-768`（安全/性能折中），并在论文中通过基准测试给出延迟与 CPU 开销对比。

### 7.2 混合策略（推荐）

推荐至少使用一种经典 DH（如 x25519）+ 一种 ML-KEM（如 ml-kem-768），并通过 RFC 9370 的密钥更新链把共享秘密逐轮注入（第 8 节给出公式）。这样：

* 若未来量子可破 DH，但 ML-KEM 仍安全：机密性仍保；
* 若 ML-KEM 出现结构性弱点，但 DH 仍安全：机密性仍保；
* 两者都破则不保（这是混合的边界）。

---

## 8. 密钥派生与密钥更新（Key Derivation and Key Updates）

### 8.1 IKE_INTERMEDIATE 的密钥更新（RFC 9370）

当执行第 n 个额外密钥交换并得到共享秘密 `SK(n)` 后，按 RFC 9370 更新：

* `SKEYSEED(n) = prf(SK_d(n-1), SK(n) | Ni | Nr)`
* `{SK_d(n) | SK_ai(n) | SK_ar(n) | SK_ei(n) | SK_er(n) | SK_pi(n) | SK_pr(n)}    = prf+(SKEYSEED(n), Ni | Nr | SPIi | SPIr)`

该链式更新使得每轮引入的新共享秘密都会影响后续 SK_*，最终影响 IKE_AUTH 与 CHILD_SA 的密钥材料。

### 8.2 本方案对 `SK(n)` 的定义

* 若 ADDKE 为 ML-KEM：`SK(n)` 为 ML-KEM 导出的 shared secret。
* 若 ADDKE 为 SM2-KEM：`SK(n)` 建议为 `r_i || r_r`（或其 KDF 输出）。

---

## 9. 证书与认证

### 9.1 “抗量子证书”位置。

* IKE_AUTH 本来就承载身份认证与证书链（CERT payload）。
* “抗量子证书”本质是把签名算法换为 PQ 签名（或复合签名），并由证书体系承载公钥与算法标识。

若采用 ML-DSA/SLH-DSA 证书，X.509 侧的算法标识已在 RFC 9881（ML-DSA）与 RFC 9909（SLH-DSA）中给出，可作为证书生态的对接依据。 ([IETF Datatracker][6])
对应算法本身来自 NIST FIPS 204/205。 ([NIST计算机安全资源中心][3])

### 9.2 IKE_AUTH 阶段的抗量子认证（PQC AUTH）

本方案要求在 IKE_AUTH 使用后量子签名完成 AUTH（例如 ML-DSA 或 SLH-DSA）：

* 发起方/响应方在 IKE_AUTH 的 CERT payload 中提供可验证的 PQ AuthCert（或其证书链）；
* AUTH 使用 PQ 签名算法对 IKEv2 的 SignedOctets（并追加 IntAuth）进行签名，以实现对 IKE_SA_INIT 与全部 IKE_INTERMEDIATE 的认证绑定。

注：SignedOctets/IntAuth 的绑定逻辑遵循 RFC 7427（数字签名框架）与 RFC 9242（中间交换的 IntAuth 扩展）；在仅使用 PSK 的场景，AUTH 也可为 MAC，但本文档的“抗量子认证”目标要求使用 PQ 签名认证。


### 9.3 双证书的加入位置

* **EncCert（加密证书）**：建议在 IKE_INTERMEDIATE #0 里发送（在 SK 内），用于后续 SM2-KEM。
* **SignCert（签名证书）**：在 IKE_INTERMEDIATE #0 提前发送并认证，以减少Dos攻击风险（提前认证仅用于 DoS/策略门控，不构成最终身份认证；最终身份认证仍由 IKE_AUTH 完成，并包含对所有中间交换的认证绑定。）。

---

## 10. strongSwan 实现建议（概要）

strongSwan 6.0 文档已经给出 RFC 9370 的配置入口：通过 `keX_` 前缀配置最多 7 个额外密钥交换。 ([StrongSwan][4])
并且 strongSwan 6.0 引入 `ml` 插件支持 ML-KEM。 ([StrongSwan][7])

### 10.1 配置示例

```conf
# swanctl.conf (示意)
connections {
  pqgm {
    local {
      auth = pubkey
      certs = authCert.pem        # 认证证书，和双证书的签名证书区分，用于IKE_AUTH阶段的认证绑定（ML-DSA）
      id = "C=CN, O=..., CN=initiator"
    }
    remote {
      auth = pubkey
      id = "C=CN, O=..., CN=responder"
    }

    # 主 KE + 额外 KE（RFC 9370）
    proposals = aes256gcm16-prfsha256-x25519-ke1_mlkem768-ke2_sm2kem

    children {
      net {
        esp_proposals = aes256gcm16
        local_ts  = 10.0.0.0/24
        remote_ts = 10.1.0.0/24
      }
    }
  }
}
```

说明：

* `ke1_mlkem768` 的写法与 strongSwan 文档一致（文档中 “keX_” 机制、以及 mlkem512/768/1024 关键字均有列出）。 ([StrongSwan][4])
* `sm2kem` 需要你新增/扩展一个 Key Exchange Method：可通过新增插件（或对接 GMSSL/openssl engine）实现 KE payload 的封装/解封装与共享秘密导出。
* IKE_INTERMEDIATE #0 的“发送 EncCert和SigCert”需要在 charon 任务流中插入一个“仅传数据的 intermediate exchange”。

---

## 11. 安全性考虑（Security Considerations）

1. **DoS 放大**：ML-KEM等后量子算法可能显著增大报文，并且后量子密码计算复杂度高。建议启用 IKEv2 分片（RFC 7383）与抗 DoS 机制。
2. **私有 Transform ID 的互通性**：SM2-KEM 若使用私有 Transform ID，仅能在同实现/同厂商/同实验环境互通；论文中应明确“实验性/私有扩展”，并在 IANA Considerations 里说明不申请 codepoint。IANA 已明确 Transform Type 4 的 1024–65535 为私有用途。
3. **证书隐私**：若在 IKE_INTERMEDIATE #0 里提前发送证书，仍在 SK 内但可被未来量子破解“历史 DH”后解密；若你希望连证书元数据都抗量子，可把 ML-KEM 的密钥更新放在证书传输之前（先做 ADDKE1，再做证书分发）。

---

## 12. IANA 考虑（IANA Considerations）

本文档不申请新的 IANA codepoint。

* ML-KEM 使用 IANA 已列出的 `ml-kem-512/768/1024` Transform IDs（35/36/37）。
* SM2-KEM 使用 Transform Type 4 私有范围（1024–65535）中的一个值（例如 60001）。

---

## 13. 参考文献（References）

### 13.1 规范性引用（Normative）

* RFC 7296：Internet Key Exchange Protocol Version 2 (IKEv2) ([IETF][8])
* RFC 9242：Intermediate IKEv2 Exchange ([IETF Datatracker][1])
* RFC 9370：Multiple Key Exchanges in IKEv2 ([IETF Datatracker][5])
* FIPS 203：ML-KEM ([NIST计算机安全资源中心][2])
* FIPS 204：ML-DSA ([NIST计算机安全资源中心][3])
* FIPS 205：SLH-DSA ([IETF Datatracker][9])

### 13.2 资料性引用（Informative）

* IANA IKEv2 Parameters（Transform Type、Notify、KE IDs 等）
* RFC 7383：IKEv2 Message Fragmentation
* RFC 8019：IKEv2 DoS protection（puzzle/cookie）
* RFC 7427：IKEv2 Digital Signatures ([Tychon][10])
* RFC 9593：Supported Authentication Methods
* RFC 9881：ML-DSA for X.509 ([IETF Datatracker][6])
* RFC 9909：SLH-DSA for X.509 ([cloudsecurityalliance.org][11])
* strongSwan 6.0 文档：proposals / keX_ / ML-KEM 插件 ([StrongSwan][4])

---

[1]: https://datatracker.ietf.org/doc/rfc9242/?utm_source=chatgpt.com "RFC 9242 - Intermediate Exchange in the Internet Key ..."
[2]: https://csrc.nist.gov/pubs/fips/203/final?utm_source=chatgpt.com "Module-Lattice-Based Key-Encapsulation Mechanism Standard"
[3]: https://csrc.nist.gov/pubs/fips/204/final?utm_source=chatgpt.com "FIPS 204, Module-Lattice-Based Digital Signature Standard"
[4]: https://docs.strongswan.org/docs/latest/config/proposals.html "Algorithm Proposals (Cipher Suites) :: strongSwan Documentation"
[5]: https://datatracker.ietf.org/doc/html/rfc9370?utm_source=chatgpt.com "RFC 9370 - Multiple Key Exchanges in the Internet ..."
[6]: https://datatracker.ietf.org/doc/rfc9909/?utm_source=chatgpt.com "RFC 9909 - Internet X.509 Public Key Infrastructure"
[7]: https://docs.strongswan.org/docs/latest/news/whatsNew.html "What’s New in strongSwan 6.0 :: strongSwan Documentation"
[8]: https://www.ietf.org/archive/id/draft-ietf-lamps-x509-slhdsa-01.html?utm_source=chatgpt.com "Algorithm Identifiers for SLH-DSA"
[9]: https://datatracker.ietf.org/doc/draft-ietf-lamps-x509-slhdsa/history/?utm_source=chatgpt.com "History for draft-ietf-lamps-x509-slhdsa -09"
[10]: https://tychon.io/the-quantum-leap-new-post-quantum-cryptography-algorithms-released/?utm_source=chatgpt.com "New Post-Quantum Cryptography Algorithms Released - Tychon"
[11]: https://cloudsecurityalliance.org/blog/2024/08/15/nist-fips-203-204-and-205-finalized-an-important-step-towards-a-quantum-safe-future?utm_source=chatgpt.com "NIST FIPS 203, 204, 205 Finalized | PQC Algorithms"
