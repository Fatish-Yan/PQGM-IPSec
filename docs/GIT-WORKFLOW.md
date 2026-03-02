# PQGM-IPSec Git 工作流程指南

本文档说明如何管理 PQGM-IPSec 项目的 Git 工作流程，确保第三方项目（strongswan、GmSSL、liboqs）的修改不会影响原始开源仓库。

---

## 📁 仓库结构

```
/home/ipsec/
├── PQGM-IPSec/          # 主仓库（你的 GitHub: Fatish-Yan/PQGM-IPSec）
│   ├── patches/         # 第三方项目修改的 patch 备份
│   ├── docs/            # 项目文档
│   ├── scripts/         # 构建和测试脚本
│   └── ...
│
├── strongswan/          # strongSwan 源码（官方仓库的本地 clone）
│   └── .git/            # remote: github.com/strongswan/strongswan
│
└── GmSSL/               # GmSSL 源码（官方仓库的本地 clone）
    └── .git/            # remote: github.com/guanzhi/GmSSL
```

---

## 🎯 核心原则

### ⚠️ 最重要的规则

1. **永远不要 push 到第三方项目的官方仓库**
   - `strongswan/` 的 remote 指向 `github.com/strongswan/strongswan`
   - `GmSSL/` 的 remote 指向 `github.com/guanzhi/GmSSL`
   - 你没有这些仓库的写权限，所以即使误操作也会被拒绝

2. **所有修改必须备份到 PQGM-IPSec 仓库**
   - 使用 `git format-patch` 或 `git diff` 导出修改
   - 存储在 `patches/` 目录
   - 提交到你的 GitHub 仓库

3. **PQGM-IPSec 仓库是唯一需要 push 的仓库**
   - 你的仓库: `github.com/Fatish-Yan/PQGM-IPSec`
   - 所有代码、文档、patch 都备份在这里

---

## 📋 工作流程

### 1. 修改第三方项目前的准备

```bash
# 确认你在正确的目录
cd /home/ipsec/strongswan   # 或 /home/ipsec/GmSSL

# 查看当前状态
git status
```

### 2. 进行代码修改

在 strongswan/ 或 GmSSL/ 目录中正常修改代码。

### 3. 提交修改到本地 Git

```bash
# 在 strongswan 或 GmSSL 目录中
git add .
git commit -m "描述你的修改"
```

### 4. **关键步骤：更新 patch 备份**

完成修改后，**必须**立即更新 patch 文件：

```bash
# ===== 更新 strongswan patch =====
# 方式1: 如果已提交，导出所有提交
git -C /home/ipsec/strongswan format-patch origin/master --stdout > \
  /home/ipsec/PQGM-IPSec/patches/strongswan/all-modifications.patch

# 方式2: 如果有未提交修改，导出差异
git -C /home/ipsec/strongswan diff > \
  /home/ipsec/PQGM-IPSec/patches/strongswan/uncommitted-modifications.patch

# ===== 更新 GmSSL patch =====
# GmSSL 通常用 diff（因为修改较小）
git -C /home/ipsec/GmSSL diff > \
  /home/ipsec/PQGM-IPSec/patches/gmssl/uncommitted-modifications.patch
```

### 5. 提交 patch 到 PQGM-IPSec 仓库

```bash
cd /home/ipsec/PQGM-IPSec

# 添加更新的 patch
git add patches/

# 提交
git commit -m "chore: update patches for [修改内容]"

# 推送到 GitHub
git push origin main
```

### 6. 分支管理策略

```bash
# 为新功能创建分支
git checkout -b feature/your-feature-name

# 开发完成后合并到 main
git checkout main
git merge feature/your-feature-name

# 删除已完成的功能分支（可选）
git branch -d feature/your-feature-name

# 推送到 GitHub
git push origin main
```

---

## 🔍 常用命令

### 查看仓库状态

```bash
# PQGM-IPSec 主仓库
cd /home/ipsec/PQGM-IPSec && git status

# strongswan
git -C /home/ipsec/strongswan status

# GmSSL
git -C /home/ipsec/GmSSL status
```

### 查看修改历史

```bash
# PQGM-IPSec 最近提交
cd /home/ipsec/PQGM-IPSec && git log --oneline -10

# strongswan 相对于官方仓库的修改
git -C /home/ipsec/strongswan log --oneline origin/master..HEAD
```

### 恢复修改（从 patch）

```bash
# 在新环境中应用 strongswan patch
cd /path/to/strongswan
git apply /home/ipsec/PQGM-IPSec/patches/strongswan/all-modifications.patch

# 在新环境中应用 GmSSL patch
cd /path/to/GmSSL
git apply /home/ipsec/PQGM-IPSec/patches/gmssl/uncommitted-modifications.patch
```

---

## 🚨 常见错误和预防

### 错误 1: 尝试 push 到官方仓库

```bash
# ❌ 错误操作
cd /home/ipsec/strongswan
git push origin master
# 输出: error: failed to push some refs to 'https://github.com/strongswan/strongswan.git'
```

**解决方案**: 这是预期行为！你没有权限，所以无法影响官方仓库。

### 错误 2: 忘记更新 patch

**后果**: 代码修改只存在本地，系统崩溃后无法恢复。

**预防**: 每次修改第三方项目后，立即更新 patch 并推送到 GitHub。

### 错误 3: 在错误目录提交

```bash
# ❌ 错误：在 PQGM-IPSec 目录提交 strongswan 代码
cd /home/ipsec/PQGM-IPSec
git add ../strongswan/  # 不要这样做！
```

**正确做法**: strongswan 的修改在 strongswan 目录提交，然后用 patch 备份。

---

## 📊 当前仓库配置

| 仓库 | 远程地址 | 本地路径 | 用途 |
|------|----------|----------|------|
| PQGM-IPSec | github.com/Fatish-Yan/PQGM-IPSec | /home/ipsec/PQGM-IPSec | 主仓库，存储所有代码、文档、patch |
| strongswan | github.com/strongswan/strongswan | /home/ipsec/strongswan | 第三方项目，修改用 patch 备份 |
| GmSSL | github.com/guanzhi/GmSSL | /home/ipsec/GmSSL | 第三方项目，修改用 patch 备份 |

---

## 🔄 同步 checklist

每完成一个开发阶段，执行以下检查：

```bash
# [ ] 1. 检查 strongswan/GmSSL 是否有未提交的修改
git -C /home/ipsec/strongswan status
git -C /home/ipsec/GmSSL status

# [ ] 2. 更新 patch 文件
git -C /home/ipsec/strongswan format-patch origin/master --stdout > \
  /home/ipsec/PQGM-IPSec/patches/strongswan/all-modifications.patch
git -C /home/ipsec/GmSSL diff > \
  /home/ipsec/PQGM-IPSec/patches/gmssl/uncommitted-modifications.patch

# [ ] 3. 在 PQGM-IPSec 仓库提交更新
cd /home/ipsec/PQGM-IPSec
git add patches/
git commit -m "chore: update patches"
git push origin main
```

---

## 📝 创建时间

2026-03-02

## 📌 记忆路径

请将此文档路径存入记忆：
`/home/ipsec/PQGM-IPSec/docs/GIT-WORKFLOW.md`

每次 git 操作前，参考此文档确保不会搞乱仓库。
