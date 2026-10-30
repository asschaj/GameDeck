# GameDeck —— 开源游戏服务器托管面板

一个基于 Docker 的开源游戏服务器托管平台：在 Linux 服务器上通过容器化方式启动/停止开源游戏服务端，提供 Web 面板进行可视化管理。

## 特性

- 🎮 **游戏容器管理**：一键启动/停止开源游戏服务端（Mindustry、Minetest 等）
- ➕ **可添加游戏清单**：内置更多游戏（Minecraft、Terraria 等），前端选择后自动构建镜像并注册，无需重启服务
- ⚡ **实时状态推送**：WebSocket 通信，游戏状态变化实时同步到前端
- 🐳 **Docker 隔离**：每个游戏运行在独立容器，环境隔离、易清理
- 📋 **日志查看**：实时查看游戏容器运行日志
- 🧹 **缓存清理**：一键清理停止的容器与无用镜像，返回释放空间
- 🖥️ **双客户端**：Web 面板 + Qt 桌面客户端（统一管理界面）
- 🔧 **可扩展**：新增游戏只需在 `GameRegistry` 注册配置 + 提供 Dockerfile

## 架构

```
前端 Web (HTML/JS)  ←WebSocket→  C++ 后端 (Boost.Beast)  →  Docker 容器 (游戏服务端)
```

## 技术栈

| 组件 | 技术 |
|---|---|
| 后端 | C++17、Boost.Beast (WebSocket)、Boost.Asio |
| 容器 | Docker |
| 前端 | 原生 HTML/CSS/JS + WebSocket |
| 构建 | CMake |

## 目录结构

```
gamedeck/
├── CMakeLists.txt
├── DESIGN.md               # 详细设计文档
├── docker/                 # 游戏镜像
│   ├── mindustry/Dockerfile
│   ├── minetest/Dockerfile
│   ├── minecraft/Dockerfile
│   └── terraria/Dockerfile
├── src/                    # C++ 后端
│   ├── main.cpp            # 入口
│   ├── Server.h/.cpp       # WebSocket 服务器
│   ├── Session.h/.cpp      # 客户端会话 + 消息处理
│   ├── GameManager.h/.cpp  # Docker 容器管理（异步）
│   ├── GameRegistry.h/.cpp # 游戏注册表
│   ├── ThreadPool.h        # 线程池（执行阻塞 Docker 命令）
│   └── Protocol.h          # 前后端协议
├── qt_client/              # Qt 桌面客户端（统一管理界面）
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── MainWindow.h/.cpp  # 主窗口
│       └── GameClient.h/.cpp  # WebSocket 客户端封装
├── tests/                  # 测试脚本
│   ├── stability_test.py   # 稳定性测试（压测+内存监控）
│   ├── stress_test.py      # 并发压测
│   ├── start_stop_test.py  # 反复启停测试
│   └── memory_watch.py     # 内存监控
└── web/                    # 前端
    ├── index.html
    ├── style.css
    └── app.js
```

## 快速开始

### 前置依赖

- Ubuntu/Debian Linux
- CMake ≥ 3.16
- g++ (C++17)
- Boost (system + Beast 头文件)
- Docker

```bash
# 安装依赖
sudo apt install build-essential cmake libboost-system-dev libboost-all-dev docker.io
```

### 构建后端

```bash
cd gamedeck
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j4
```

### 构建游戏镜像

```bash
# Mindustry
docker build -t gamedeck/mindustry:latest docker/mindustry/

# Minetest
docker build -t gamedeck/minetest:latest docker/minetest/
```

### 启动服务

```bash
./build/gamedeck_server 8080
```

### 打开前端

浏览器访问 `http://localhost:8080`（或通过前端静态服务器托管 `web/` 目录）。

### 构建 Qt 桌面客户端

```bash
# 依赖
sudo apt install qtbase5-dev libqt5websockets5-dev

# 构建
cd qt_client
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j4

# 运行
./build/gamedeck_client
```

Qt 客户端与 Web 面板功能等价，通过 QWebSocket 连接后端（默认 `127.0.0.1:8080`）。

## 前后端协议

WebSocket 文本消息（JSON）：

```jsonc
// 客户端 → 服务端
{ "type": "list_games" }
{ "type": "list_available" }                    // 可添加的游戏清单
{ "type": "install_game", "game": "minecraft" } // 构建镜像并注册游戏
{ "type": "start_game", "game": "mindustry" }
{ "type": "stop_game",  "game": "mindustry" }
{ "type": "get_logs",   "game": "mindustry" }
{ "type": "clean_cache" }

// 服务端 → 客户端
{ "type": "game_list", "games": [{"name":"mindustry","display":"Mindustry","status":"running"}] }
{ "type": "available_list", "games": [{"name":"minecraft","display":"Minecraft","description":"..."}] }
{ "type": "install_result", "game": "minecraft", "success": true, "message": "..." }
{ "type": "game_status", "game": "mindustry", "status": "running" }
{ "type": "game_logs", "game": "mindustry", "logs": "..." }
{ "type": "clean_result", "freed_mb": 0 }
{ "type": "error", "message": "..." }
```

## 扩展新游戏

1. 在 `docker/<game>/Dockerfile` 编写镜像
2. 在 `GameRegistry.cpp` 构造函数中注册游戏配置（名称、镜像、端口映射、数据卷）

```cpp
GameConfig c;
c.name = "your_game";
c.display = "Your Game";
c.description = "描述";
c.image = "gamedeck/your_game:latest";
c.ports = {"25565:25565/tcp"};
c.hasVolume = true;
c.volumePath = "your_game-data:/data";
c.dockerfile = "docker/your_game";
c.installed = false;  // false = 出现在「可添加游戏」清单，用户选择后 install
games_[c.name] = c;
```

- `installed = true`：镜像默认已构建，直接出现在「已安装游戏」列表
- `installed = false`：出现在「可添加游戏」清单，用户点击「安装」后后端执行 `docker build` 并自动转为已安装

## 技术亮点

- **线程池异步化**：Docker CLI 命令（阻塞 IO）通过线程池异步执行，避免阻塞 asio 事件循环；回调通过 `net::post` 投递回 io_context 线程，保证 WebSocket 写操作线程安全
- **并发安全**：游戏状态批量查询用 shared_ptr 计数器汇总，strand 语义保证写操作串行

## 测试

项目内置 4 个测试脚本，覆盖功能、并发、稳定性：

```bash
# 一体化稳定性测试（压测 + 内存监控，推荐）
python3 tests/stability_test.py 20 60    # 20 并发，60 秒

# 并发压测 + 突发连接
python3 tests/stress_test.py 20 60

# 反复启停 + 混合操作（竞态测试）
python3 tests/start_stop_test.py 10

# 独立内存监控
python3 tests/memory_watch.py 30 5
```

**实测结果**（20 并发 60 秒）：

| 指标 | 结果 |
|---|---|
| 总请求 | 2618 次 |
| 失败请求 | 0 次 |
| 内存 RSS | 4.73MB → 4.73MB（增长 0.00MB）|

验证了线程池异步设计与引用计数生命周期管理的正确性，无内存泄漏、无崩溃。

## 待办

- [x] Docker 命令异步化（线程池），避免阻塞事件循环
- [ ] 容器资源占用展示（docker stats）
- [ ] 容器资源限制（CPU/内存）
- [ ] 用户认证
- [ ] 游戏配置持久化（保存到 JSON 文件）

## License

MIT
