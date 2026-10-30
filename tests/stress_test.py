#!/usr/bin/env python3
"""
GameDeck 长时间多进程压测脚本

测试目标：
1. 并发连接稳定性：多客户端同时连接、同时发命令
2. 长时间稳定性：持续发请求，监控服务不崩、响应正常
3. 异步正确性：大量并发请求下，响应是否仍能正确对应

用法：
  python3 stress_test.py [并发数] [持续时间秒]
  默认：20 并发，60 秒
"""

import json
import sys
import threading
import time
import websocket


class StressResult:
    def __init__(self):
        self.lock = threading.Lock()
        self.total_requests = 0
        self.success = 0
        self.failed = 0
        self.errors = []

    def add_success(self):
        with self.lock:
            self.total_requests += 1
            self.success += 1

    def add_fail(self, err):
        with self.lock:
            self.total_requests += 1
            self.failed += 1
            if len(self.errors) < 10:
                self.errors.append(err)


def worker(worker_id, duration, result, stop_event):
    """单个工作线程：持续发 list_games 请求"""
    try:
        ws = websocket.create_connection("ws://127.0.0.1:8080", timeout=10)
    except Exception as e:
        result.add_fail(f"worker{worker_id} 连接失败: {e}")
        return

    start = time.time()
    while not stop_event.is_set() and time.time() - start < duration:
        try:
            ws.send(json.dumps({"type": "list_games"}))
            resp = ws.recv()
            # 验证响应是合法的 game_list
            data = json.loads(resp)
            if data.get("type") == "game_list":
                result.add_success()
            else:
                result.add_fail(f"worker{worker_id} 收到异常响应: {resp[:50]}")
        except Exception as e:
            result.add_fail(f"worker{worker_id} 请求失败: {e}")
            # 尝试重连
            try:
                ws.close()
            except Exception:
                pass
            try:
                ws = websocket.create_connection("ws://127.0.0.1:8080", timeout=10)
            except Exception:
                break

        time.sleep(0.05)  # 每次请求间隔，避免打满

    try:
        ws.close()
    except Exception:
        pass


def burst_test(result):
    """突发测试：一次性大量并发连接同时发请求"""
    def single_burst():
        try:
            ws = websocket.create_connection("ws://127.0.0.1:8080", timeout=10)
            ws.send(json.dumps({"type": "list_games"}))
            resp = ws.recv()
            data = json.loads(resp)
            if data.get("type") == "game_list":
                result.add_success()
            else:
                result.add_fail(f"burst 异常响应: {resp[:50]}")
            ws.close()
        except Exception as e:
            result.add_fail(f"burst 失败: {e}")

    threads = []
    for _ in range(50):  # 50 个突发连接
        t = threading.Thread(target=single_burst)
        t.start()
        threads.append(t)
    for t in threads:
        t.join()


def main():
    concurrency = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    duration = int(sys.argv[2]) if len(sys.argv) > 2 else 60

    print(f"=== GameDeck 长时间压测 ===")
    print(f"并发数: {concurrency}, 持续时间: {duration} 秒\n")

    result = StressResult()
    stop_event = threading.Event()

    # 启动工作线程
    threads = []
    for i in range(concurrency):
        t = threading.Thread(target=worker, args=(i, duration, result, stop_event))
        t.start()
        threads.append(t)

    # 压测期间每隔 10 秒打印进度，并做一次突发测试
    start = time.time()
    last_report = start
    burst_count = 0
    while time.time() - start < duration:
        time.sleep(1)
        elapsed = time.time() - start
        if time.time() - last_report >= 10:
            with result.lock:
                print(f"[{int(elapsed)}s] 请求总数={result.total_requests}, "
                      f"成功={result.success}, 失败={result.failed}")
            # 突发测试
            burst_test(result)
            burst_count += 1
            last_report = time.time()

    # 停止所有线程
    stop_event.set()
    for t in threads:
        t.join(timeout=5)

    # 最终统计
    print("\n=== 压测完成 ===")
    print(f"持续时间: {duration} 秒")
    print(f"并发连接数: {concurrency}")
    print(f"突发测试次数: {burst_count} (每次 50 连接)")
    print(f"总请求数: {result.total_requests}")
    print(f"成功: {result.success}")
    print(f"失败: {result.failed}")
    if result.errors:
        print("\n错误样本（前 10 条）:")
        for e in result.errors:
            print(f"  - {e}")

    # 判定
    if result.failed == 0:
        print("\n✅ 压测通过：无失败请求")
        return 0
    else:
        fail_rate = result.failed / max(result.total_requests, 1) * 100
        print(f"\n⚠️ 存在失败请求，失败率 {fail_rate:.2f}%")
        return 1


if __name__ == "__main__":
    sys.exit(main())
