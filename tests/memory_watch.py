#!/usr/bin/env python3
"""
GameDeck 内存监控脚本

长时间运行时定期采样服务进程内存（RSS），检测是否存在内存泄漏。

用法：
  python3 memory_watch.py [采样次数] [间隔秒]
  默认：30 次采样，每次间隔 5 秒
"""

import subprocess
import sys
import time


def get_rss_mb(pid):
    """读取进程 RSS 内存（MB）"""
    try:
        with open(f"/proc/{pid}/status") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    # VmRSS:   12345 kB
                    return int(line.split()[1]) / 1024.0
    except Exception:
        return -1
    return -1


def find_pid():
    """查找 gamedeck_server 进程 PID（精确匹配二进制，排除 bash 包装）"""
    try:
        out = subprocess.check_output(
            ["pgrep", "-f", "gamedeck_server"], text=True).strip()
        pids = []
        for p in out.split():
            if not p.strip().isdigit():
                continue
            pid = int(p)
            # 通过 /proc/<pid>/comm 精确匹配二进制名
            try:
                with open(f"/proc/{pid}/comm") as f:
                    comm = f.read().strip()
                if comm == "gamedeck_server":
                    pids.append(pid)
            except Exception:
                continue
        return pids[0] if pids else -1
    except Exception:
        return -1


def main():
    samples = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    interval = int(sys.argv[2]) if len(sys.argv) > 2 else 5

    pid = find_pid()
    if pid < 0:
        print("未找到 gamedeck_server 进程，请先启动服务")
        return 1

    print(f"=== GameDeck 内存监控 ===")
    print(f"进程 PID: {pid}, 采样 {samples} 次, 间隔 {interval} 秒\n")

    readings = []
    for i in range(samples):
        rss = get_rss_mb(pid)
        readings.append(rss)
        print(f"[{i+1}/{samples}] RSS = {rss:.2f} MB")
        time.sleep(interval)

    # 分析内存趋势
    first = readings[0]
    last = readings[-1]
    peak = max(readings)
    diff = last - first

    print("\n=== 内存分析 ===")
    print(f"初始 RSS: {first:.2f} MB")
    print(f"最终 RSS: {last:.2f} MB")
    print(f"峰值 RSS: {peak:.2f} MB")
    print(f"增长: {diff:.2f} MB")

    # 判定：增长超过 10MB 视为可疑泄漏
    if diff > 10:
        print(f"\n⚠️ 内存增长 {diff:.2f} MB，可能存在内存泄漏")
        return 1
    else:
        print(f"\n✅ 内存稳定，无泄漏迹象（增长 {diff:.2f} MB）")
        return 0


if __name__ == "__main__":
    sys.exit(main())
