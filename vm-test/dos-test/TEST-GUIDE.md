# DoS 门控机制对比测试指南

## 测试概述

本测试旨在对比 PQ-GM-IKEv2 协议在高频 DoS 攻击下，有无早期门控机制的资源消耗差异。

### 测试环境
- **Initiator (攻击方)**: 192.168.172.134 (initiator.pqgm.test)
- **Responder (被攻击方)**: 需要确认 IP 地址

### 测试参数
- 攻击频率: ~50 次/秒
- 持续时间: 60 秒
- 监控指标: CPU、内存、IKE SA 数量

---

## 第一阶段：无门控基线测试

### Responder 指令

请在 Responder VM 上执行以下命令：

```bash
# 1. 确认 charon 运行状态
sudo ipsec status

# 2. 创建监控脚本目录
mkdir -p ~/dos-test-results

# 3. 启动 CPU 监控（60秒）
pidstat -p $(pidof charon) 1 60 > ~/dos-test-results/cpu_baseline_no_gating.log 2>&1 &

# 4. 启动内存监控
while true; do
    echo "$(date +%s) $(ps -p $(pidof charon) -o rss --no-headers 2>/dev/null || echo 0)"
    sleep 1
done > ~/dos-test-results/mem_baseline_no_gating.log 2>&1 &

# 5. 记录测试开始
echo "测试开始: $(date)" > ~/dos-test-results/test_record.log
echo "测试类型: 无门控基线" >> ~/dos-test-results/test_record.log

# 6. 通知发起方可以开始攻击
echo "Responder 已就绪，等待攻击..."
```

### Initiator 执行

攻击脚本已准备好，执行：

```bash
cd /home/ipsec/PQGM-IPSec/vm-test/dos-test
./dos_attack.sh
```

### 攻击结束后 (Responder)

```bash
# 停止内存监控
pkill -f "while true.*rss"

# 记录测试结束
echo "测试结束: $(date)" >> ~/dos-test-results/test_record.log

# 查看结果摘要
echo "CPU 平均占用率:"
grep "Average" ~/dos-test-results/cpu_baseline_no_gating.log || tail -5 ~/dos-test-results/cpu_baseline_no_gating.log

echo ""
echo "内存数据:"
tail -5 ~/dos-test-results/mem_baseline_no_gating.log
```

---

## 第二阶段：注入门控代码

### Responder 端修改代码

需要修改 `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_init.c` 文件。

门控代码将在后续提供。

---

## 第三阶段：有门控对比测试

重复第一阶段步骤，但使用修改后的代码。

---

## 数据收集

### 需要收集的文件

1. `cpu_baseline_no_gating.log` - 无门控 CPU 数据
2. `mem_baseline_no_gating.log` - 无门控内存数据
3. `cpu_baseline_with_gating.log` - 有门控 CPU 数据
4. `mem_baseline_with_gating.log` - 有门控内存数据

### 数据格式转换

```bash
# 转换 CPU 数据为 CSV
awk '/Average/{next} /charon/{print NR","$4}' ~/dos-test-results/cpu_baseline_no_gating.log > cpu_baseline.csv

# 转换内存数据为 CSV
awk '{print NR","$2}' ~/dos-test-results/mem_baseline_no_gating.log > mem_baseline.csv
```