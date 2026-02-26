# PQGM-IKEv2 项目文档

> **项目名称**: PQ-GM-IKEv2: 抗量子 IPSec 协议设计与实现
> **项目类型**: 硕士论文项目
> **最后更新**: 2026-02-26

---

## 1. 项目需求

### 1.1 研究目标
设计并实现一个融合后量子密码学和中国国密算法的 IKEv2/IPSec VPN 协议栈。

### 1.2 核心功能
1. **SM2/SM3/SM4 国密算法支持** (GM/T 0002-0004-2012)
2. **后量子密钥交换** (Kyber/KEM 系列)
3. **与 strongSwan 6.0.4 集成**
4. **性能测试与对比分析**

### 1.3 论文章节规划
- 第3章: 相关技术综述
- 第4章: 协议设计
- 第5章: 系统实现与实验分析

---

## 2. 开发环境

### 2.1 系统信息
```bash
OS: Ubuntu 22.04
Kernel: Linux 6.8.0-101-generic
Shell: bash
```

### 2.2 核心依赖
| 组件 | 版本 | 用途 |
|------|------|------|
| strongSwan | 6.0.4 | IPSec VPN 实现 |
| GmSSL | 3.1.3 Dev | 国密算法库 |
| gcc | - | 编译器 |

### 2.3 关键路径
```
strongSwan 源码:    /home/ipsec/strongswan
gmalg 插件:         /home/ipsec/strongswan/src/libstrongswan/plugins/gmalg
项目文档:           /home/ipsec/PQGM-IPSec
GmSSL 安装:         /usr/local/lib
```

### 2.4 系统配置
```bash
Sudo 密码: 1574a
```

---

## 3. 项目架构

### 3.1 gmalg 插件结构
```
gmalg/
├── gmalg_plugin.c/h      # 插件入口，算法注册
├── gmalg_hasher.c/h      # SM3 哈希算法
├── gmalg_crypter.c/h     # SM4 分组加密
├── gmalg_signer.c/h      # SM2 签名算法
├── gmalg_prf.c/h         # SM3 伪随机函数
└── Makefile.am           # 构建配置
```

### 3.2 算法 ID 分配 (私有使用空间)
```c
/* GM/T 0004-2012 SM3 Hash Algorithm */
#define HASH_SM3        1032

/* GM/T 0002-2012 SM4 Block Cipher */
#define ENCR_SM4_ECB    1040
#define ENCR_SM4_CBC    1041
#define ENCR_SM4_CTR    1042

/* GM/T 0003-2012 SM2 Signature Algorithm */
#define AUTH_SM2        1050

/* GM/T 0003-2012 SM2 Key Exchange */
#define KE_SM2          1051

/* PRF using SM3 */
#define PRF_SM3         1052
```

---

## 4. 实现状态

### 4.1 已完成 ✅

| 算法 | 状态 | 功能测试 | 性能测试 |
|------|------|----------|----------|
| SM3 Hash | ✅ | ✅ 通过 | ✅ 443 MB/s |
| SM3 PRF | ✅ | ✅ 通过 | ✅ 3.7M ops/s |
| SM4 ECB | ✅ | ✅ 通过 | ✅ 189 MB/s |
| SM4 CBC | ✅ | ✅ 通过 | ✅ 161-175 MB/s |
| SM2 Signer | ✅ | ⏳ 待测试 | - |

### 4.2 待实现 ⏳
- SM4 CTR 模式
- SM2-KEM 密钥交换
- 后量子 KEM (Kyber) 集成

---

## 5. 错误避坑记录

### 5.1 编译相关

####坑点 1: HAVE_GMSSL 未定义
**现象**: `#ifdef HAVE_GMSSL` 条件为假，导致算法未注册
**原因**: configure.ac 中 HAVE_GMSSL 定义位置错误
**解决**: 手动在 config.h 中添加 `#define HAVE_GMSSL 1`
**文件**: `/home/ipsec/strongswan/config.h`

####坑点 2: PLUGIN_PROVIDE 宏参数错误
**现象**: `error: macro "_PLUGIN_FEATURE_CRYPTER" requires 3 arguments, but only 2 given`
**原因**: CRYPTER 类型需要 3 个参数 (type, algo, keysize)
**解决**:
```c
// 错误
PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_ECB)
// 正确
PLUGIN_PROVIDE(CRYPTER, ENCR_SM4_ECB, 16)
```

####坑点 3: SIGNER 类型参数数量
**现象**: SIGNER 只需 2 个参数，不是 3 个
**解决**:
```c
// 正确
PLUGIN_PROVIDE(SIGNER, AUTH_SM2)  // 只有 2 个参数
```

####坑点 4: 单体模式编译链接错误
**现象**: `make[5]: *** 没有规则可制作目标"../../../../src/libstrongswan/libstrongswan.la"`
**原因**: Makefile.am 在单体模式下仍尝试链接 libstrongswan.la
**解决**: 使用条件 LIBADD
```makefile
if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-gmalg.la
libstrongswan_gmalg_la_LIBADD = -L/usr/local/lib -lgmssl
else
plugin_LTLIBRARIES = libstrongswan-gmalg.la
libstrongswan_gmalg_la_LIBADD = \
    $(top_builddir)/src/libstrongswan/libstrongswan.la \
    -L/usr/local/lib -lgmssl
endif
```

####坑点 5: 宏定义中的注释导致编译错误
**现象**: `error: excess elements in struct initializer`
**原因**: 宏定义中包含 `/* comment */` 在 INIT 宏中使用时出现问题
**解决**: 将注释放在单独行或移除
```c
// 错误
#define GMALG_SM2_PRIV_KEY_SIZE 32  /* 256-bit private key */

// 正确
/* SM2 key and signature sizes */
#define GMALG_SM2_PRIV_KEY_SIZE 32
```

### 5.2 GmSSL API 相关

####坑点 6: SM2_DEFAULT_ID_LEN vs SM2_DEFAULT_ID_LENGTH
**现象**: `error: 'SM2_DEFAULT_ID_LEN' undeclared`
**原因**: GmSSL 3.1.3 使用 `SM2_DEFAULT_ID_LENGTH`
**解决**: 使用正确的常量名 `SM2_DEFAULT_ID_LENGTH`

####坑点 7: sm2_key_init 函数不存在
**现象**: `error: implicit declaration of function 'sm2_key_init'`
**原因**: GmSSL 3.1.3 没有 sm2_key_init 函数
**解决**: 使用 memset 初始化为 0
```c
memset(&this->sm2_key, 0, sizeof(SM2_KEY));
```

####坑点 8: sm3_hash 函数不存在
**现象**: `error: implicit declaration of function 'sm3_hash'`
**原因**: GmSSL 3.1.3 没有直接的 sm3_hash 函数
**解决**: 使用 SM3_CTX
```c
SM3_CTX ctx;
sm3_init(&ctx);
sm3_update(&ctx, data, len);
sm3_finish(&ctx, digest);
```

####坑点 9: memxor 类型冲突
**现象**: strongSwan 和 GmSSL 的 memxor 定义不同
**原因**: 两个库都定义了 memxor 但参数类型略有差异
**解决**: 不要包含 `<gmssl/mem.h>`，只包含需要的 GmSSL 头文件

####坑点 10: sm2_key_set_private_key 参数类型
**现象**: `error: passing argument 2 of 'sm2_key_set_private_key' from incompatible pointer type`
**原因**: sm2_z256_t 是 uint64_t[4] 类型
**解决**:
```c
// 错误
sm2_key_set_private_key(&this->sm2_key, (sm2_z256_t*)ptr)

// 正确
sm2_key_set_private_key(&this->sm2_key, (const uint64_t*)ptr)
```

####坑点 11: SM2_PRIVATE_KEY_SIZE 宏冲突
**现象**: 自定义的 SM2_KEY_SIZE 与 GmSSL 的 SM2_PRIVATE_KEY_SIZE (96) 冲突
**原因**: GmSSL 定义的 SM2_PRIVATE_KEY_SIZE 是 96（包含额外数据），不是纯私钥大小
**解决**: 使用不同的宏名前缀
```c
#define GMALG_SM2_PRIV_KEY_SIZE 32
#define GMALG_SM2_PUB_KEY_SIZE 65
```

### 5.3 插件加载相关

####坑点 12: 插件加载后立即卸载
**现象**: 日志显示 `unloading plugin 'gmalg' without loaded features`
**原因**: HAVE_GMSSL 未定义，导致特征数组为空
**解决**: 确保 config.h 中定义了 HAVE_GMSSL

### 5.4 strongSwan 框架相关

####坑点 13: INIT 宏初始化顺序
**现象**: 结构体初始化错误
**原因**: INIT 宏中的字段顺序必须与结构体定义一致
**解决**: 确保 INIT 中初始化所有必要的字段
```c
INIT(this,
    .public = {
        .signer_interface = {
            // ... 接口函数
        },
    },
    .has_private_key = FALSE,
    .key_size = GMALG_SM2_PRIV_KEY_SIZE,
);
```

---

## 6. 测试工具

### 6.1 功能测试
```bash
# 测试程序位置
/home/ipsec/PQGM-IPSec/test_gmalg
/home/ipsec/PQGM-IPSec/benchmark_gmalg
```

### 6.2 运行测试
```bash
cd /home/ipsec/PQGM-IPSec
LD_LIBRARY_PATH=/usr/local/lib:/home/ipsec/strongswan/src/libstrongswan/.libs \
./test_gmalg
```

---

## 7. 常用命令

### 7.1 编译
```bash
cd /home/ipsec/strongswan
make -j$(nproc)
```

### 7.2 安装
```bash
cd /home/ipsec/strongswan
sudo make install
```

### 7.3 重启服务
```bash
sudo systemctl restart strongswan
# 或
sudo charon-systemd stop
sudo charon-systemd start
```

### 7.4 查看插件状态
```bash
swanctl --stats
# 查找 gmalg 在 loaded plugins 列表中
```

---

## 8. Git 仓库

### 8.1 本地仓库
```
路径: /home/ipsec/PQGM-IPSec
远程: https://github.com/Fatish-Yan/PQGM-IPSec
分支: main
```

### 8.2 提交历史
```
a1a6da5 修复 gmalg 插件编译和加载问题
3e84312 添加 SM3/SM4 算法测试程序和测试结果
```

---

## 9. 下一步计划

1. ✅ 安装编译好的 strongSwan
2. ⏳ 测试 SM2 signer 功能
3. ⏳ 实现 SM4 CTR 模式
4. ⏳ 实现 SM2-KEM 密钥交换
5. ⏳ 集成后量子 KEM 算法
6. ⏳ 完成论文实验章节

---

## 10. 性能测试结果

### SM3 哈希性能
- 吞吐量: **443.35 MB/s**
- 单次哈希: 0.141 ms

### SM3 PRF 性能
- 操作速率: **3,701,173 次/秒**

### SM4 ECB 性能
- 加密: 189.45 MB/s
- 解密: 189.94 MB/s

### SM4 CBC 性能
- 加密: 161.27 MB/s
- 解密: 174.76 MB/s

---

## 11. 参考资料

### 11.1 国密标准
- GM/T 0002-2012: SM4 分组密码算法
- GM/T 0003-2012: SM2 椭圆曲线公钥密码算法
- GM/T 0004-2012: SM3 密码杂凑算法

### 11.2 相关链接
- strongSwan: https://www.strongswan.org/
- GmSSL: https://github.com/guanzhi/GmSSL
- IKEv2 RFC: RFC 7296

---

**文档版本**: 1.0
**维护者**: Claude + 用户
**最后更新**: 2026-02-26
