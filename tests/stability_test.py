#!/usr/bin/env python3
"""
GameDeck 一体化稳定性测试（压测 + 内存监控）

单进程同时做：并发压测 + 服务进程内存采样，检测长时间运行是否
内存泄漏、是否崩溃、异步是否错乱。

用法：
  python3 stability_test.py [并发数] [持续时间秒]
  默认：20 并发，60 秒
"""

import json
import subprocess
import sys
import threading
import time
import websocket


def find_server_pid():
    """精确查找 gamedeck_server 进程 PID"""
    try:
        out = subprocess.check_output(
            ["pgrep", "-f", "gamedeck_server"], text=True).strip()
        for p in out.split():
            if not p.strip().isdigit():
                continue
            pid = int(p)
            try:
                with open(f"/proc/{pid}/comm") as f:
                    comm = f.read().strip()
                if comm == "gamedeck_server":
                    return pid
            except Exception:
                continue
    except Exception:
        pass
    return -1


def get_rss_mb(pid):
    try:
        with open(f"/proc/{pid}/status") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) / 1024.0
    except Exception:
        return -1
    return -1


def main():
    concurrency = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 60

    pid = find_server_pid()
    if pid < 0:
        print("未找到 gamedeck_server 进程，请先启动服务")
        return 1

    print(f"=== GameDeck 稳定性测试 ===")
    print(f"服务 PID: {pid}, 并发: {concurrency}, 时长: {duration}s\n")

    stats = {"total": 0, "failed": 0}
    stats_lock = threading.Lock()
    stop_event = threading.Event()
    errors = []

    def worker(wid):
        try:
            ws = websocket.create_connection("ws://127.0.0.1:8080", timeout=10)
        except Exception as e:
            with stats_lock:
                errors.append(f"worker{wid} 连接失败: {e}")
            return
        while not stop_event.is_set():
            try:
                ws.send(json.dumps({"type": "list_games"}))
                resp = ws.recv()
                data = json.loads(resp)
                with stats_lock:
                    stats["total"] += 1
                    if data.get("type") != "game_list":
                        stats["failed"] += 1
                        if len(errors) < 5:
                            errors.append(f"worker{wid} 异常响应: {resp[:50]}")
            except Exception as e:
                with stats_lock:
                    stats["failed"] += 1
                    if len(errors) < 5:
                        errors.append(f"worker{wid}: {e}")
                try:
                    ws.close()
                except Exception:
                    pass
                try:
                    ws = websocket.create_connection("ws://127.0.0.1:8080", timeout=10)
                except Exception:
                    break
            time.sleep(0.05)
        try:
            ws.close()
        except Exception:
            pass

    # 启动压测线程
    threads = []
    for i in range(concurrency):
        t = threading.Thread(target=worker, args=(i,))
        t.start()
        threads.append(t)

    # 主线程做内存采样 + 进度打印
    start = time.time()
    rss_samples = []
    while time.time() - start < duration:
        time.sleep(5)
        rss = get_rss_mb(pid)
        rss_samples.append(rss)
        elapsed = int(time.time() - start)
        with stats_lock:
            total = stats["total"]
            failed = stats["failed"]
        print(f"[{elapsed}s] RSS={rss:.2f}MB, 请求={total}, 失败={failed}")

    stop_event.set()
    for t in threads:
        t.join(timeout=5)

    # 最终统计
    print("\n=== 结果 ===")
    if rss_samples:
        first_rss = rss_samples[0]
        last_rss = rss_samples[-1]
        peak_rss = max(rss_samples)
        growth = last_rss - first_rss
        print(f"内存: 初始 {first_rss:.2f}MB -> 最终 {last_rss:.2f}MB (峰值 {peak_rss:.2f}MB, 增长 {growth:.2f}MB)")
    else:
        growth = 0
        print("内存: 无采样数据")

    print(f"请求: 总 {stats['total']}, 失败 {stats['failed']}")

    if errors:
        print("错误样本:")
        for e in errors:
            print(f"  - {e}")

    # 判定
    leak = growth > 10
    if stats["failed"] == 0 and not leak:
        print("\n✅ 稳定性测试通过：无失败请求，无内存泄漏")
        return 0
    else:
        print("\n⚠️ 存在问题：")
        if stats["failed"] > 0:
            print(f"  - 失败请求 {stats['failed']} 次")
        if leak:
            print(f"  - 内存增长 {growth:.2f}MB，疑似泄漏")
        return 1


if __name__ == "__main__":
    sys.exit(main())
