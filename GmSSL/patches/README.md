# GmSSL 修改说明

## 修改文件列表

### 1. src/bn.c
**问题**: 编译错误 `unknown type name 'uint32_t'`
**修复**: 添加 `#include <stdint.h>` 在文件开头

### 2. src/kyber.c
**问题**: 链接错误 `multiple definition of 'zeta'`
**修复**:
- 将 `int16_t zeta[256]` 改为 `static int16_t zeta[256]`
- 保持 `init_zeta()` 为非静态，供工具程序使用

### 3. include/gmssl/kyber.h
**修改**: 移除 `zeta` 的外部声明，避免多定义错误

## 应用补丁

在 GmSSL 根目录执行：

```bash
git apply patches/0001-fix-bn-stdint.patch
git apply patches/0002-fix-kyber-zeta.patch
```

## 编译安装

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```
