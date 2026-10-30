# GameDeck —— 开源游戏服务器托管面板

> 一个基于 Docker 的开源游戏服务器托管平台：在 Linux 服务器上通过容器化方式启动/停止开源游戏服务端，提供 Web 面板进行可视化管理。

## 一、项目定位

- **场景**：在 Linux 服务器（Ubuntu）上运行开源游戏服务端（Mindustry、Minetest 等）
- **后端**：C++ 服务，负责 Docker 容器管理（启动/停止/清理缓存）
- **前端**：Web 页面，展示游戏列表，一键启动/关闭，实时状态推送
- **通信**：WebSocket（实时状态推送）
- **目标**：开源放 GitHub，展示 Linux 后端 + 容器化 + 网络编程能力

## 二、整体架构

```
┌─────────────┐     WebSocket      ┌──────────────────┐
│   前端 Web   │ ◄────────────────► │   C++ 后端服务    │
│ (游戏列表/    │   (JSON 消息)     │  (GameDeck Core)  │
│  启停按钮)   │                   │                  │
└─────────────┘                   │  ┌────────────┐  │
                                  │  │ Docker 管理 │  │
                                  │  │ (子进程调用)│  │
                                  │  └─────┬──────┘  │
                                  └────────┼─────────┘
                                           │ docker run/stop/rm
                                  ┌────────▼─────────┐
                                  │   Docker 容器     │
                                  │  (游戏服务端)     │
                                  │  Mindustry 等    │
                                  └──────────────────┘
```

## 三、技术选型

| 组件 | 技术 | 理由 |
|---|---|---|
| 后端语言 | C++17 | 契合简历技术栈 |
| WebSocket | Boost.Beast | Boost 标准库，无需额外依赖 |
| 网络模型 | 多线程 + 异步 | Beast 异步模型 |
| 容器管理 | Docker CLI（子进程） | 无需 Docker SDK，简单可靠 |
| 前端 | HTML/JS + WebSocket | 同学负责 |
| 构建 | CMake | 通用 |

## 四、核心功能

1. **游戏列表**：展示可管理的游戏（名称、描述、当前状态 running/stopped）
2. **启动游戏**：通过 `docker run` 启动指定游戏容器
3. **停止游戏**：通过 `docker stop` / `docker rm` 停止并清理容器
4. **状态推送**：游戏状态变化时通过 WebSocket 实时推送给前端
5. **缓存清理**：清理游戏产生的临时文件、日志、未使用的 Docker 镜像

## 五、目录结构

```
gamedeck/
├── CMakeLists.txt              # 构建配置
├── DESIGN.md                   # 本文档
├── README.md                   # 项目说明
├── docker/                     # Docker 相关
│   ├── mindustry/Dockerfile    # Mindustry 游戏镜像
│   └── minetest/Dockerfile     # Minetest 游戏镜像
├── src/                        # C++ 后端源码
│   ├── main.cpp                # 入口
│   ├── Server.h/.cpp           # WebSocket 服务器
│   ├── Session.h/.cpp          # 单个客户端连接会话
│   ├── GameManager.h/.cpp      # 游戏进程/容器管理
│   ├── GameRegistry.h/.cpp     # 游戏注册表（可管理游戏列表）
│   └── Protocol.h              # 前后端协议定义
└── web/                        # 前端（同学负责）
    ├── index.html
    ├── app.js
    └── style.css
```

## 六、前后端协议（WebSocket JSON）

### 客户端 → 服务端

```json
{ "type": "list_games" }
{ "type": "start_game", "game": "mindustry" }
{ "type": "stop_game",  "game": "mindustry" }
{ "type": "clean_cache" }
```

### 服务端 → 客户端

```json
{ "type": "game_list", "games": [{"name":"mindustry","display":"Mindustry","status":"running","port":6567}] }
{ "type": "game_status", "game": "mindustry", "status": "running" }
{ "type": "log", "level": "info", "message": "..." }
{ "type": "clean_result", "freed_mb": 123 }
```

## 七、待办里程碑

- [x] 需求与架构设计
- [ ] Docker 游戏镜像
- [ ] C++ 后端骨架（WebSocket + 游戏管理）
- [ ] 前端页面
- [ ] README 与开源准备
