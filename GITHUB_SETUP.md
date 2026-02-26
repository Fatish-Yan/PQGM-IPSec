# GitHub 远程仓库设置指南

本指南将帮助您将本地 PQGM-IKEv2 项目连接到 GitHub 远程仓库。

---

## 方式一：手动创建 GitHub 仓库（推荐）

### 步骤 1: 在 GitHub 上创建新仓库

1. 访问 https://github.com/new
2. 填写仓库信息：
   - **Repository name**: `PQGM-IPSec`
   - **Description**: `抗量子IPSec协议设计与实现 - PQ-GM-IKEv2`
   - **可见性**: 选择 `Private`（私有）或 `Public`（公开）
   - **不要**勾选 "Add a README file"
   - **不要**勾选 "Add .gitignore"
   - **不要**选择 "Choose a license"
3. 点击 **Create repository**

### 步骤 2: 连接本地仓库到 GitHub

创建仓库后，GitHub 会显示仓库地址，格式为：
```
https://github.com/你的用户名/PQGM-IPSec.git
```

然后执行以下命令：

```bash
cd /home/ipsec/PQGM-IPSec

# 添加远程仓库（替换下面的 URL）
git remote add origin https://github.com/你的用户名/PQGM-IPSec.git

# 验证远程仓库
git remote -v

# 推送到 GitHub
git push -u origin main
```

### 步骤 3: 验证推送成功

访问你的 GitHub 仓库页面，应该能看到所有文件已上传。

---

## 方式二：使用 GitHub CLI (gh)

如果安装了 GitHub CLI 工具，可以使用命令行创建：

```bash
# 安装 GitHub CLI
sudo apt install -y gh

# 登录 GitHub
gh auth login

# 创建仓库并推送
cd /home/ipsec/PQGM-IPSec
gh repo create PQGM-IPSec --public --source=. --remote=origin --push
```

---

## 方式三：使用 GitHub Personal Access Token

### 步骤 1: 创建 Personal Access Token

1. 访问 https://github.com/settings/tokens
2. 点击 **Generate new token** → **Generate new token (classic)**
3. 配置 token：
   - **Note**: `PQGM-IPSec Development`
   - **Expiration**: 选择过期时间
   - **Scopes**: 勾选 `repo`（完整的仓库访问权限）
4. 点击 **Generate token**
5. **重要**: 复制生成的 token（只显示一次！）

### 步骤 2: 使用 Token 推送

```bash
cd /home/ipsec/PQGM-IPSec

# 添加远程仓库
git remote add origin https://YOUR_TOKEN@github.com/你的用户名/PQGM-IPSec.git

# 或者使用 git credential helper
git config credential.helper store
git push -u origin main
# 然后输入用户名和 token（密码位置）
```

---

## 常用 Git 命令

### 日常工作流程

```bash
# 查看状态
git status

# 查看提交历史
git log --oneline --graph --all

# 添加修改的文件
git add 文件名
# 或添加所有修改
git add -a

# 提交修改
git commit -m "描述你的修改"

# 推送到远程
git push

# 拉取远程更新
git pull
```

### 分支管理

```bash
# 创建新分支
git branch feature-分支名

# 切换分支
git checkout 分支名

# 创建并切换分支
git checkout -b feature-分支名

# 合并分支
git merge 分支名

# 删除分支
git branch -d 分支名
```

---

## SSH 密钥配置（可选）

为了更安全、更方便地推送代码，建议配置 SSH 密钥：

```bash
# 生成 SSH 密钥
ssh-keygen -t ed25519 -C "your_email@example.com"

# 查看公钥
cat ~/.ssh/id_ed25519.pub

# 将公钥添加到 GitHub:
# 1. 复制公钥内容
# 2. 访问 https://github.com/settings/ssh
# 3. 点击 "New SSH key"
# 4. 粘贴公钥内容

# 使用 SSH URL 连接远程仓库
git remote set-url origin git@github.com:你的用户名/PQGM-IPSec.git

# 测试连接
ssh -T git@github.com
```

---

## 安全注意事项

1. **永远不要提交私钥文件** (`.key`, `pem` 私钥部分)
   - 项目已配置 `.gitignore` 忽略常见私钥文件
   - 提交前检查：`git status` 确认没有敏感文件

2. **不要提交大文件** (`.pdf`, `.docx`, 虚拟机镜像等)
   - 已在 `.gitignore` 中配置

3. **私有仓库推荐**: 对于学术论文项目，建议使用私有仓库

4. **定期备份**: 除了 GitHub，建议定期备份到其他位置

---

## 项目协作

如果需要与他人协作：

```bash
# 添加协作者
# 1. 访问仓库 Settings → Collaborators
# 2. 点击 Add people
# 3. 输入协作用户名

# Fork 工作流（开源项目常用）
# 1. 其他人 Fork 你的仓库
# 2. 他们修改后提交 Pull Request
# 3. 你审查并合并 PR
```

---

## 故障排除

### 推送被拒绝

```bash
# 拉取远程更新后再推送
git pull --rebase origin main
git push origin main
```

### 认证失败

```bash
# 清除凭据缓存
git credential-cache exit

# 或配置新的凭据
git config credential.helper store
git push  # 会提示重新输入用户名和token
```

### 找不到远程仓库

```bash
# 检查远程配置
git remote -v

# 重新添加远程
git remote remove origin
git remote add origin 正确的仓库地址
```

---

创建日期: 2025-02-26
最后更新: 2025-02-26
