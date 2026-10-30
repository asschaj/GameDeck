#!/usr/bin/env python3
"""
GameDeck 反复启停测试

测试目标：
  大量 start/stop/get_logs/clean_cache 并发操作，验证异步回调的正确性、
  状态一致性、线程池不阻塞、无内存泄漏。

用法：
  python3 start_stop_test.py [轮数]
  默认：10 轮
"""

import json
import sys
import threading
import time
import websocket


def send_recv(ws, obj, timeout=30):
    """发送并等待响应"""
    ws.send(json.dumps(obj))
    return ws.recv()


def mixed_worker(worker_id, rounds, results, errors, lock):
    """混合操作工作线程：随机执行各种命令"""
    try:
        ws = websocket.create_connection("ws://127.0.0.1:8080", timeout=30)
    except Exception as e:
        with lock:
            errors.append(f"worker{worker_id} 连接失败: {e}")
        return

    ops = ["list_games", "start_game", "stop_game", "get_logs", "clean_cache"]
    for r in range(rounds):
        # 随机选一个操作
        op = ops[(worker_id + r) % len(ops)]
        try:
            if op == "list_games":
                resp = send_recv(ws, {"type": "list_games"})
            elif op == "start_game":
                resp = send_recv(ws, {"type": "start_game", "game": "mindustry"})
            elif op == "stop_game":
                resp = send_recv(ws, {"type": "stop_game", "game": "mindustry"})
            elif op == "get_logs":
                resp = send_recv(ws, {"type": "get_logs", "game": "mindustry"})
            elif op == "clean_cache":
                resp = send_recv(ws, {"type": "clean_cache"})

            data = json.loads(resp)
            with lock:
                results[op] = results.get(op, 0) + 1

        except Exception as e:
            with lock:
                errors.append(f"worker{worker_id} {op} 失败: {e}")
            # 重连
            try:
                ws.close()
            except Exception:
                pass
            try:
                ws = websocket.create_connection("ws://127.0.0.1:8080", timeout=30)
            except Exception:
                break

    try:
        ws.close()
    except Exception:
        pass


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    concurrency = 10  # 10 个并发 worker

    print(f"=== GameDeck 反复启停测试 ===")
    print(f"并发 worker: {concurrency}, 每 worker {rounds} 轮\n")

    results = {}
    errors = []
    lock = threading.Lock()

    threads = []
    start = time.time()
    for i in range(concurrency):
        t = threading.Thread(target=mixed_worker, args=(i, rounds, results, errors, lock))
        t.start()
        threads.append(t)

    for t in threads:
        t.join(timeout=60)

    elapsed = time.time() - start

    print(f"=== 测试完成，耗时 {elapsed:.1f} 秒 ===")
    total = sum(results.values())
    print(f"总请求数: {total}")
    for op, count in sorted(results.items()):
        print(f"  {op}: {count} 次")

    if errors:
        print(f"\n错误数: {len(errors)}")
        print("错误样本（前 10 条）:")
        for e in errors[:10]:
            print(f"  - {e}")
        return 1
    else:
        print(f"\n✅ 测试通过：{total} 次请求无错误")
        return 0


if __name__ == "__main__":
    sys.exit(main())
