# Patches 目录

本目录存储对第三方项目的修改补丁，所有修改都备份在 PQGM-IPSec 仓库中，不会推送到原始项目。

## 目录结构

```
patches/
├── strongswan/          # strongSwan 修改
│   └── all-modifications.patch  # 所有修改的完整补丁
└── gmssl/               # GmSSL 修改
    └── uncommitted-modifications.patch  # Kyber 相关修改
```

## strongSwan 修改内容

`all-modifications.patch` 包含以下功能：

### gmalg 插件（国密算法）
- SM3 哈希算法
- SM4 对称加密（ECB/CBC/CTR）
- SM2 签名算法
- SM2-KEM 密钥封装

### mldsa 插件（后量子签名）
- ML-DSA-65 签名器
- ML-DSA 私钥加载器
- ML-DSA 公钥加载器（支持混合证书）

### 核心修改
- credential_manager.c: ML-DSA 私钥回退查找
- credential_manager.c: 混合证书公钥提取
- public_key.c: scheme_map 添加 ML-DSA
- IKE_INTERMEDIATE: 双向证书交换
- RFC 9370: 多重密钥交换支持

## GmSSL 修改内容

`uncommitted-modifications.patch` 包含：
- Kyber（ML-KEM）相关的小修改

## 应用补丁

```bash
# 应用 strongSwan 补丁
cd /path/to/strongswan
git apply /path/to/PQGM-IPSec/patches/strongswan/all-modifications.patch

# 应用 GmSSL 补丁
cd /path/to/GmSSL
git apply /path/to/PQGM-IPSec/patches/gmssl/uncommitted-modifications.patch
```

## 更新补丁

当 strongSwan 或 GmSSL 有新的修改时，重新生成补丁：

```bash
# strongSwan
git -C /home/ipsec/strongswan format-patch origin/master --stdout > patches/strongswan/all-modifications.patch

# GmSSL (未提交修改)
git -C /home/ipsec/GmSSL diff > patches/gmssl/uncommitted-modifications.patch
```

---

*创建时间: 2026-03-02*
