# Docker 测试操作问题清单

> **目的**: 记录 Docker 测试环境中遇到的操作性问题，避免重复犯错

**更新日期**: 2026-03-03

---

## 1. 库文件挂载问题

### 问题描述
- Docker 容器挂载宿主机的 `/usr/local/` 目录
- 修改 strongSwan 代码后重新编译安装，但容器内仍使用旧版本

### 症状
```
# 宿主机重新编译安装
cd /home/ipsec/strongswan && make install

# 容器内仍显示旧时间戳
docker exec pqgm-initiator ls -l /usr/local/lib/ipsec/plugins/libstrongswan-mldsa.so
```

### 原因
- Docker 挂载是 bind mount，直接映射宿主机目录
- 但进程可能缓存了旧的 `.so` 文件

### 解决方案
```bash
# 必须重启容器使新库生效
docker-compose down && docker-compose up -d

# 验证库文件时间戳
docker exec pqgm-initiator stat /usr/local/lib/ipsec/plugins/libstrongswan-mldsa.so
```

---

## 2. 配置文件同步问题

### 问题描述
- 修改 `docker/*/config/swanctl.conf` 后
- 容器内看不到新配置

### 原因
- 配置文件在容器启动时被读取
- 运行时修改不会自动生效

### 解决方案
```bash
# 修改配置后必须重启容器
docker-compose down && docker-compose up -d

# 或重新加载配置（部分情况）
docker exec pqgm-initiator swanctl --load-all
```

---

## 3. Responder 未加载凭证

### 问题描述
```
[IKE] received NO_PROPOSAL_CHOSEN notify error
```

### 原因
- 只在 Initiator 端执行了 `swanctl --load-all`
- Responder 端忘记加载凭证

### 解决方案
```bash
# 测试前确保两端都加载了凭证
docker exec pqgm-responder swanctl --load-all
docker exec pqgm-initiator swanctl --load-all

# 然后再发起连接
docker exec pqgm-initiator swanctl --initiate --child net
```

---

## 4. 日志查看不完整

### 问题描述
- 使用 `docker logs` 只看到最后几行
- 错误信息被截断

### 解决方案
```bash
# 查看完整日志
docker logs pqgm-initiator 2>&1 | less

# 只看 ML-DSA 相关
docker logs pqgm-initiator 2>&1 | grep -i "ML-DSA"

# 查看最近的 N 行
docker logs --tail 100 pqgm-initiator

# 实时跟踪日志
docker logs -f pqgm-initiator
```

---

## 5. 容器启动时间不足

### 问题描述
```
docker-compose up -d
docker exec pqgm-initiator swanctl --initiate --child net
# 错误: charon not ready
```

### 原因
- 容器启动需要时间
- charon 进程初始化需要时间

### 解决方案
```bash
# 启动后等待几秒
docker-compose up -d
sleep 6  # 等待 charon 完全启动

# 然后再操作
docker exec pqgm-initiator swanctl --initiate --child net
```

---

## 6. 网络配置错误

### 问题描述
- 容器间无法通信
- 连接超时

### 常见错误
```yaml
# docker-compose.yml 中 IP 配置错误
services:
  initiator:
    networks:
      pqgm_net:
        ipv4_address: 172.28.0.10  # 错误: 应该是 172.28.0.10/16
```

### 解决方案
```bash
# 检查网络配置
docker network inspect docker_pqgm_net

# 测试连通性
docker exec pqgm-initiator ping 172.28.0.20
docker exec pqgm-responder ping 172.28.0.10
```

---

## 7. 文件权限问题

### 问题描述
```
cannot open file '/usr/local/etc/swanctl/private/xxx.pem': Permission denied
```

### 原因
- 宿主机上文件权限不正确
- Docker 容器内用户 ID 不同

### 解决方案
```bash
# 在宿主机上设置正确权限
chmod 644 /home/ipsec/PQGM-IPSec/docker/*/certs/x509/*.pem
chmod 600 /home/ipsec/PQGM-IPSec/docker/*/certs/private/*.pem

# 或者在容器内检查
docker exec pqgm-initiator ls -la /usr/local/etc/swanctl/private/
```

---

## 8. 证书/私钥文件名错误

### 问题描述
```
loading private key from 'xxx_key.bin' failed
```

### 常见错误
- swanctl.conf 中指定的文件名与实际文件名不匹配
- 大小写错误
- 路径错误

### 解决方案
```bash
# 检查实际文件名
docker exec pqgm-initiator ls -la /usr/local/etc/swanctl/private/
docker exec pqgm-initiator ls -la /usr/local/etc/swanctl/x509/

# 与 swanctl.conf 对比
docker exec pqgm-initiator cat /usr/local/etc/swanctl/swanctl.conf
```

---

## 9. 编译缓存问题

### 问题描述
- 修改源码后重新编译，但行为不变
- `.o` 文件没有重新生成

### 解决方案
```bash
# 清理后重新编译
cd /home/ipsec/strongswan
make clean
make -j$(nproc)
sudo make install
sudo ldconfig

# 然后重启容器
cd /home/ipsec/PQGM-IPSec/docker
docker-compose down && docker-compose up -d
```

---

## 10. Git 提交前忘记更新 patch

### 问题描述
- strongSwan 代码修改了但 patch 没更新
- 下次从 patch 恢复时丢失修改

### 解决方案
```bash
# 每次 strongSwan 修改后，提交前更新 patch
git -C /home/ipsec/strongswan format-patch origin/master --stdout > \
  /home/ipsec/PQGM-IPSec/patches/strongswan/all-modifications.patch

# 然后一起提交
git add patches/strongswan/all-modifications.patch
```

---

## 标准测试流程

为了避免上述问题，遵循以下标准流程：

```bash
# 1. 修改 strongSwan 代码
# 2. 重新编译
cd /home/ipsec/strongswan
make -j$(nproc) && sudo make install && sudo ldconfig

# 3. 更新 patch
git -C /home/ipsec/strongswan format-patch origin/master --stdout > \
  /home/ipsec/PQGM-IPSec/patches/strongswan/all-modifications.patch

# 4. 重启容器
cd /home/ipsec/PQGM-IPSec/docker
docker-compose down && docker-compose up -d

# 5. 等待启动
sleep 6

# 6. 加载凭证（两端）
docker exec pqgm-responder swanctl --load-all
docker exec pqgm-initiator swanctl --load-all

# 7. 发起测试
docker exec pqgm-initiator swanctl --initiate --child net

# 8. 查看结果
docker logs pqgm-initiator 2>&1 | grep -i "ML-DSA\|established"
docker logs pqgm-responder 2>&1 | grep -i "ML-DSA\|established"

# 9. 检查 SA
docker exec pqgm-initiator swanctl --list-sas
```

---

## 文档更新记录

| 日期 | 更新内容 |
|------|---------|
| 2026-03-03 | 创建文档，记录 10 个常见问题 |
