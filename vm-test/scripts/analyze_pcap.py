#!/usr/bin/env python3
"""
PCAP分析脚本 - 分析完整IKEv2 + ESP通信流程

功能:
  1. IKE_SA_INIT (UDP 500) - 初始密钥交换
  2. IKE_INTERMEDIATE (UDP 4500) - 中间交换（证书分发、KEM）
  3. IKE_AUTH (UDP 4500) - 认证
  4. ESP通信 (协议50或UDP 4500封装) - 数据传输

用法: python3 analyze_pcap.py <pcap_file>
"""

import sys
import subprocess
import json
from datetime import datetime
from collections import defaultdict

def run_tshark(pcap_file, display_filter, fields):
    """运行tshark提取数据"""
    cmd = [
        'tshark', '-r', pcap_file,
        '-Y', display_filter,
        '-T', 'fields',
    ]
    for f in fields:
        cmd.extend(['-e', f])
    cmd.extend(['-E', 'header=n', '-E', 'separator=,'])

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return result.stdout.strip().split('\n')
    except subprocess.CalledProcessError as e:
        print(f"Error running tshark: {e}")
        return []
    except FileNotFoundError:
        print("Error: tshark not found. Install with: sudo apt install tshark")
        return []

def parse_packets(lines, min_fields=5):
    """解析tshark输出"""
    packets = []
    for line in lines:
        if not line:
            continue
        fields = line.split(',')
        if len(fields) >= min_fields:
            packets.append(fields)
    return packets

def analyze_ike_traffic(pcap_file):
    """分析IKE流量（UDP 500/4500）"""
    lines = run_tshark(pcap_file,
        'udp.port == 500 or udp.port == 4500',
        ['frame.number', 'frame.time_epoch', 'frame.len', 'ip.src', 'ip.dst',
         'udp.srcport', 'udp.dstport'])

    packets = []
    for fields in parse_packets(lines, 7):
        try:
            packets.append({
                'frame': int(fields[0]),
                'time': float(fields[1]),
                'len': int(fields[2]),
                'src': fields[3],
                'dst': fields[4],
                'srcport': int(fields[5]),
                'dstport': int(fields[6])
            })
        except (ValueError, IndexError):
            continue

    return packets

def analyze_esp_traffic(pcap_file):
    """分析ESP流量"""
    # 捕获原生ESP (协议50) 和 NAT-T封装的ESP (UDP 4500中的ESP)
    lines = run_tshark(pcap_file,
        'esp or (udp.port == 4500 and data.len > 0)',
        ['frame.number', 'frame.time_epoch', 'frame.len', 'ip.src', 'ip.dst',
         'esp.spi', 'udp.srcport'])

    packets = []
    for fields in parse_packets(lines, 5):
        try:
            pkt = {
                'frame': int(fields[0]),
                'time': float(fields[1]),
                'len': int(fields[2]),
                'src': fields[3],
                'dst': fields[4],
                'spi': fields[5] if len(fields) > 5 and fields[5] else None
            }
            packets.append(pkt)
        except (ValueError, IndexError):
            continue

    return packets

def classify_ike_stage(packets):
    """分类IKE阶段"""
    stages = {
        'IKE_SA_INIT': [],      # UDP 500
        'IKE_INTERMEDIATE': [], # UDP 4500, 在AUTH之前
        'IKE_AUTH': [],         # UDP 4500, 最后阶段
        'UNKNOWN': []
    }

    udp4500_packets = []

    for pkt in packets:
        if pkt['srcport'] == 500 or pkt['dstport'] == 500:
            stages['IKE_SA_INIT'].append(pkt)
        elif pkt['srcport'] == 4500 or pkt['dstport'] == 4500:
            udp4500_packets.append(pkt)
        else:
            stages['UNKNOWN'].append(pkt)

    # UDP 4500包按时间排序，前2/3归入INTERMEDIATE，后1/3归入AUTH
    if udp4500_packets:
        udp4500_packets.sort(key=lambda x: x['time'])
        split_idx = int(len(udp4500_packets) * 0.67)
        stages['IKE_INTERMEDIATE'] = udp4500_packets[:split_idx]
        stages['IKE_AUTH'] = udp4500_packets[split_idx:]

    return stages

def analyze_pcap(pcap_file):
    """完整分析PCAP文件"""

    print(f"\n分析文件: {pcap_file}")
    print("="*60)

    # 分析IKE流量
    ike_packets = analyze_ike_traffic(pcap_file)

    # 分析ESP流量
    esp_packets = analyze_esp_traffic(pcap_file)

    if not ike_packets and not esp_packets:
        print("未找到任何数据包")
        return None

    # 分类IKE阶段
    ike_stages = classify_ike_stage(ike_packets)

    # 确定角色
    if ike_packets:
        initiator_ip = ike_packets[0]['src']
    elif esp_packets:
        initiator_ip = esp_packets[0]['src']
    else:
        initiator_ip = "unknown"

    # 计算统计信息
    all_packets = ike_packets + esp_packets
    if all_packets:
        first_time = min(p['time'] for p in all_packets)
        last_time = max(p['time'] for p in all_packets)
        total_duration_ms = (last_time - first_time) * 1000
    else:
        first_time = last_time = 0
        total_duration_ms = 0

    # IKE统计
    ike_stats = {
        'total_packets': len(ike_packets),
        'total_bytes': sum(p['len'] for p in ike_packets),
        'initiator_packets': len([p for p in ike_packets if p['src'] == initiator_ip]),
        'initiator_bytes': sum(p['len'] for p in ike_packets if p['src'] == initiator_ip),
        'responder_packets': len([p for p in ike_packets if p['src'] != initiator_ip]),
        'responder_bytes': sum(p['len'] for p in ike_packets if p['src'] != initiator_ip]),
    }

    # 各阶段统计
    stage_stats = {}
    for stage, pkts in ike_stages.items():
        if pkts:
            stage_first = min(p['time'] for p in pkts)
            stage_last = max(p['time'] for p in pkts)
            stage_stats[stage] = {
                'packets': len(pkts),
                'bytes': sum(p['len'] for p in pkts),
                'duration_ms': round((stage_last - stage_first) * 1000, 1),
                'relative_start_ms': round((stage_first - first_time) * 1000, 1)
            }

    # ESP统计
    esp_stats = {
        'total_packets': len(esp_packets),
        'total_bytes': sum(p['len'] for p in esp_packets),
        'initiator_packets': len([p for p in esp_packets if p['src'] == initiator_ip]),
        'initiator_bytes': sum(p['len'] for p in esp_packets if p['src'] == initiator_ip),
        'responder_packets': len([p for p in esp_packets if p['src'] != initiator_ip]),
        'responder_bytes': sum(p['len'] for p in esp_packets if p['src'] != initiator_ip]),
    }

    if esp_packets:
        esp_first = min(p['time'] for p in esp_packets)
        esp_last = max(p['time'] for p in esp_packets)
        esp_stats['duration_ms'] = round((esp_last - esp_first) * 1000, 1)
        esp_stats['start_after_ike_ms'] = round((esp_first - first_time) * 1000, 1)

    # 汇总
    stats = {
        'pcap_file': pcap_file,
        'initiator_ip': initiator_ip,
        'first_packet_time': datetime.fromtimestamp(first_time).isoformat() if first_time else None,
        'last_packet_time': datetime.fromtimestamp(last_time).isoformat() if last_time else None,
        'total_duration_ms': round(total_duration_ms, 1),
        'ike': ike_stats,
        'ike_stages': stage_stats,
        'esp': esp_stats
    }

    return stats

def print_report(stats):
    """打印分析报告"""
    print("\n" + "="*60)
    print("IKEv2 + ESP 流量分析报告")
    print("="*60)

    print(f"\n文件: {stats['pcap_file']}")
    print(f"发起方IP: {stats['initiator_ip']}")
    print(f"总时长: {stats['total_duration_ms']} ms")
    print(f"时间范围: {stats['first_packet_time']} ~ {stats['last_packet_time']}")

    print("\n--- IKE 握手统计 ---")
    ike = stats['ike']
    print(f"总包数: {ike['total_packets']}, 总字节: {ike['total_bytes']}")
    print(f"发起方: {ike['initiator_packets']} 包, {ike['initiator_bytes']} 字节")
    print(f"响应方: {ike['responder_packets']} 包, {ike['responder_bytes']} 字节")

    print("\n--- IKE 阶段分解 ---")
    for stage, s in stats['ike_stages'].items():
        print(f"{stage}:")
        print(f"  包数: {s['packets']}, 字节: {s['bytes']}")
        print(f"  时长: {s['duration_ms']} ms, 开始: +{s['relative_start_ms']} ms")

    print("\n--- ESP 数据通信 ---")
    esp = stats['esp']
    if esp['total_packets'] > 0:
        print(f"总包数: {esp['total_packets']}, 总字节: {esp['total_bytes']}")
        print(f"发起方: {esp['initiator_packets']} 包, {esp['initiator_bytes']} 字节")
        print(f"响应方: {esp['responder_packets']} 包, {esp['responder_bytes']} 字节")
        print(f"时长: {esp.get('duration_ms', 0)} ms")
        print(f"IKE完成后开始: +{esp.get('start_after_ike_ms', 0)} ms")
    else:
        print("无ESP流量")

    print("\n" + "="*60)

def main():
    if len(sys.argv) < 2:
        print("用法: python3 analyze_pcap.py <pcap_file>")
        print("\n分析完整IKEv2握手和ESP通信流程")
        sys.exit(1)

    pcap_file = sys.argv[1]
    stats = analyze_pcap(pcap_file)

    if stats:
        print_report(stats)

        # 保存JSON
        json_file = pcap_file.replace('.pcap', '_analysis.json')
        with open(json_file, 'w') as f:
            json.dump(stats, f, indent=2, ensure_ascii=False)
        print(f"\n分析结果已保存: {json_file}")

if __name__ == '__main__':
    main()
