#!/usr/bin/env bash
# GameDeck 一键编译 + 运行脚本
#
# 用法：
#   ./run.sh [端口]          # 自动编译并运行（默认端口 8080）
#   ./run.sh 8889            # 编译并运行在 8889 端口
#   ./run.sh 8889 --build    # 强制重新编译后运行
#
# 说明：
#   - 首次运行会自动执行 cmake 配置 + make 编译
#   - 后续运行若源码无改动则跳过编译，直接启动
#   - 每次修改代码后，重新执行本脚本即可编译并运行
#   - 前台运行时日志直接打印到终端（Ctrl+C 停止）
#   - 后台运行并把日志写文件：./run.sh 8889 > server.log 2>&1 &

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# 默认端口
PORT="${1:-8080}"
# 若非数字，按默认端口处理
if ! [[ "$PORT" =~ ^[0-9]+$ ]]; then
    PORT=8080
fi

BUILD_DIR="build"
BINARY="$BUILD_DIR/gamedeck_server"

echo "==================== GameDeck ===================="
echo "端口: $PORT"
echo "=================================================="

# 1. 首次配置 cmake
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "[1/3] 首次配置 CMake..."
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi

# 2. 编译（make 会自动检测改动，无改动则秒过）
echo "[2/3] 编译中（无改动则跳过）..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

# 3. 停止占用该端口的旧进程，避免端口冲突
echo "[3/3] 清理旧进程并启动..."

# 找到占用目标端口的进程（TCP 监听）
OLD_PID=$(fuser -n tcp "$PORT" 2>/dev/null | tr -s ' ' '\n' | sort -u | paste -sd' ' -)
if [ -n "$OLD_PID" ]; then
    echo "    发现占用端口 $PORT 的进程: $OLD_PID"
    # 先尝试 SIGTERM 优雅停止
    kill $OLD_PID 2>/dev/null || true
    sleep 1
    # 若仍未释放，SIGKILL 强杀
    if fuser -n tcp "$PORT" >/dev/null 2>&1; then
        echo "    SIGTERM 未生效，使用 SIGKILL..."
        kill -9 $OLD_PID 2>/dev/null || true
        sleep 1
    fi
    # 最终校验端口是否释放
    if fuser -n tcp "$PORT" >/dev/null 2>&1; then
        echo "错误: 端口 $PORT 仍被占用，可能进程属于其他用户（如 root）无权限关闭。"
        echo "请手动处理，例如:"
        echo "  sudo fuser -k $PORT/tcp"
        echo "或换个端口启动: ./run.sh <其他端口>"
        exit 1
    fi
    echo "    旧进程已停止"
fi

echo "启动 gamedeck_server，监听端口 $PORT（Ctrl+C 退出）"
echo "浏览器访问: http://localhost:$PORT"
echo "----------------------------------------------"

# Docker 权限检测：
# 若当前用户已在 docker 组，但当前会话未生效（加入组后未重新登录），
# 则提示重新登录。不建议用 sg/newgrp 绕行，会制造混乱的进程树导致清理困难。
if ! docker info >/dev/null 2>&1; then
    if id -nG "$USER" 2>/dev/null | grep -qw docker; then
        echo ""
        echo "⚠️  警告: 当前会话没有 docker 组权限（你的账号已在 docker 组，但未重新登录生效）。"
        echo "    docker 相关操作（启动/停止游戏）会报 permission denied。"
        echo ""
        echo "    请二选一解决："
        echo "    1) 退出当前终端重新登录（推荐，一劳永逸）"
        echo "    2) 或临时执行: newgrp docker  再运行本脚本"
        echo ""
        echo "    （服务本身仍会启动，但管理游戏容器会失败）"
        echo ""
    else
        echo ""
        echo "⚠️  警告: 当前用户 '$USER' 不在 docker 组，无法管理 Docker 容器。"
        echo "    请执行: sudo usermod -aG docker $USER  然后重新登录"
        echo ""
    fi
fi

exec "$BINARY" "$PORT" "$SCRIPT_DIR/web"
