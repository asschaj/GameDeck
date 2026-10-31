# GameDeck 开发说明书（面向 Java 开发同学）

> 本文档用于项目交接 / 协作开发。看完后你应该能：
> 1. 在本机把项目跑起来
> 2. 看懂前后端通信协议
> 3. 知道现在缺什么、可以做什么、改哪几个文件

---

## 一、项目是什么

**GameDeck** —— 一个开源游戏服务器托管面板。

用一句话说清楚：**在 Linux 服务器上，用 Docker 把开源游戏服务端（Mindustry、Minetest…）跑起来，然后通过一个网页/桌面面板点按钮就能启动、停止、看日志。**

```
玩家用游戏客户端 ──UDP/TCP──> Docker 容器（游戏服务端）
管理员用浏览器 ──WebSocket──> C++ 后端 ──docker CLI──> 容器
```

- **GitHub 地址**：https://github.com/asschaj/GameDeck
- **默认分支**：`main`；开发分支：`develop`
- **开源协议**：MIT
- **当前技术栈**：C++17 + Boost.Beast（WebSocket/HTTP）+ Docker + 原生 HTML/JS + Qt 桌面客户端

> 说明：项目主体是 C++，但**前后端完全通过 JSON over WebSocket 通信**，协议很简单，
> 用 Java 写一个客户端 / 甚至用 Java 重写后端都完全可行，下文会给出具体方案。

---

## 二、当前已完成的功能

| 功能 | 状态 | 说明 |
|---|---|---|
| 已安装游戏列表 | ✅ | 显示游戏、描述、运行状态、客户端下载入口 |
| 可添加游戏列表 | ✅ | 显示未构建镜像的游戏，可一键安装 |
| 一键安装游戏 | ✅ | 后端执行 `docker build`，成功后持久化到 `games.state` |
| 启动 / 停止游戏 | ✅ | 异步执行 `docker run` / `docker stop` |
| 查看容器日志 | ✅ | `docker logs --tail 100` |
| 清理缓存 | ✅ | 清理停止的容器 + 悬空镜像，返回释放空间 |
| Web 面板 | ✅ | 后端同端口托管静态页面（HTTP + WebSocket 复用 8889） |
| Qt 桌面客户端 | ✅ | QWebSocket，功能与 Web 面板等价 |
| 运行日志 | ✅ | 带毫秒时间戳，覆盖 HTTP / WS / 所有操作 |
| 压测 / 稳定性测试 | ✅ | 20 并发 60 秒，2618 次请求，0 失败，内存零增长 |

---

## 三、跑起来（5 分钟）

### 3.1 依赖

```bash
sudo apt install build-essential cmake libboost-all-dev docker.io
```

### 3.2 Docker 权限（重要，90% 的报错都是这个）

```bash
sudo usermod -aG docker $USER
# 关键：必须重新登录（退出 SSH 再进）才生效！
# 验证：
docker ps
```
> 若报 `permission denied while trying to connect to the docker API`，
> 就是组权限没生效，**重新登录**即可，不要用 `sudo` 跑整个服务（会有坑）。

### 3.3 编译 + 启动（一条命令）

```bash
git clone https://github.com/asschaj/GameDeck.git
cd GameDeck
git checkout develop

./run.sh 8889        # 自动 cmake 配置 → 编译 → 清理旧进程 → 启动
```

浏览器打开 **http://localhost:8889** 即可（HTTP 页面和 WebSocket 在同一个端口）。

### 3.4 构建两个游戏镜像（可选，不构建就是「可添加游戏」）

```bash
docker build -t gamedeck/mindustry:latest docker/mindustry/
docker build -t gamedeck/minetest:latest  docker/minetest/
```

### 3.5 验证启动成功

```bash
ss -ulnp | grep 30000          # Minetest 的 UDP 端口
docker ps | grep gamedeck      # 容器运行中
```

游戏客户端连接：Luanti/Min​etest 客户端 → 加入游戏 → 填 `虚拟机IP:30000`。

---

## 四、代码结构：改什么东西去哪里改

```
gamedeck/
├── src/                       # ★ C++ 后端（主要改动区）
│   ├── main.cpp               # 入口：解析端口 / 静态目录
│   ├── Server.h/.cpp          # 监听端口、accept 连接
│   ├── Session.h/.cpp         # ★★ 最核心：HTTP 静态文件 + WebSocket 消息分发
│   ├── GameManager.h/.cpp     # ★ Docker 容器操作（启动/停止/日志/构建/清理）
│   ├── GameRegistry.h/.cpp    # ★ 游戏注册表（新增游戏改这里）
│   ├── ThreadPool.h           # 线程池，跑阻塞的 docker CLI
│   └── Protocol.h             # ★ 消息类型常量 + GameStatus 枚举
├── docker/<game>/Dockerfile   # 每个游戏的镜像定义
├── web/                       # 前端（index.html / app.js / style.css）
├── qt_client/                 # Qt 桌面客户端
├── tests/                     # Python 压测脚本
└── run.sh                     # 一键编译启动
```

### 4.1 最常见的三个改动

**① 加一个新游戏**（改 2 处，不用改后端逻辑）

1. 写 `docker/your_game/Dockerfile`
2. 在 `src/GameRegistry.cpp` 构造函数里注册：

```cpp
GameConfig c;
c.name        = "your_game";                    // 唯一 ID
c.display     = "Your Game";                    // 展示名
c.description = "一句话描述";
c.image       = "gamedeck/your_game:latest";
c.ports       = {"25565:25565/tcp"};            // 端口映射，可多个
c.hasVolume   = true;
c.volumePath  = "your_game-data:/data";         // 数据卷，重启不丢档
c.dockerfile  = "docker/your_game";
c.downloadUrl = "https://官网地址";              // 玩家下载客户端用
c.installed   = false;                          // false = 出现在「可添加游戏」
games_[c.name] = c;
```

**② 加一个新的 WebSocket 消息类型**（改 3 处）

1. `src/Protocol.h` → `namespace msg` 加常量
2. `src/Session.cpp` → `handleMessage()` 加 `else if (type == msg::kXxx)` 分支
3. `web/app.js` → `handleMessage()` 里处理返回结果

**③ 改 Docker 操作**（只改 `GameManager.cpp`）

所有容器操作都在这里：`startGameAsync` / `stopGameAsync` / `getLogsAsync` /
`buildImageAsync` / `cleanCacheAsync` / `queryStatusAsync`。
它们都跑在线程池里，通过回调返回结果（回调会被 `net::post` 投递回 io_context 线程）。

---

## 五、通信协议（重点）

### 5.1 传输层

- 协议：**WebSocket**（RFC 6455）
- 地址：`ws://<host>:<port>`，**与 HTTP 页面同端口**（默认 8889）
- 数据格式：**JSON 文本帧**（不是二进制帧）
- 握手：普通 HTTP Upgrade，后端用 `websocket::is_upgrade()` 判断后分流

### 5.2 客户端 → 服务端（7 种请求）

| type | 附带字段 | 含义 |
|---|---|---|
| `list_games` | — | 查询已安装游戏 + 实时状态 |
| `list_available` | — | 查询可添加（镜像未构建）的游戏 |
| `install_game` | `game` | 构建镜像并注册为已安装 |
| `start_game` | `game` | 启动容器 |
| `stop_game` | `game` | 停止容器 |
| `get_logs` | `game` | 取最近 100 行日志 |
| `clean_cache` | — | 清理停止的容器与悬空镜像 |

```jsonc
{ "type": "start_game", "game": "minetest" }
```

### 5.3 服务端 → 客户端（9 种响应）

| type | 字段 | 说明 |
|---|---|---|
| `game_list` | `games[]` | 已安装游戏数组 |
| `available_list` | `games[]` | 可添加游戏数组 |
| `install_result` | `game`,`success`,`message` | 安装结果 |
| `game_status` | `game`,`status` | 状态变更：`running` / `stopped` |
| `game_logs` | `game`,`logs` | 日志文本（已做 JSON 转义） |
| `clean_result` | `freed_mb` | 释放空间（MB，整数） |
| `log` | `level`,`message` | 通用日志推送 |
| `error` | `message` | 错误信息（中文） |
| — | — | 未知 type 会收到 `error` |

### 5.4 两个列表里每个游戏的字段（注意字段名是下划线风格）

```jsonc
// game_list 单项
{
  "name": "minetest",
  "display": "Minetest",
  "description": "开源沙盒游戏（Minecraft 替代）",
  "download_url": "https://www.minetest.net/downloads/",   // 可能为空字符串
  "status": "running"                                       // running / stopped / unknown
}

// available_list 单项（没有 status 字段）
{
  "name": "minecraft",
  "display": "Minecraft (Paper)",
  "description": "经典沙盒游戏（Paper 服务端）",
  "download_url": "https://www.minecraft.net/zh-hans/download"
}
```

### 5.5 一条完整交互示例

```
客户端 → {"type":"list_games"}
服务端 → {"type":"game_list","games":[
            {"name":"minetest","display":"Minetest","description":"...",
             "download_url":"https://www.minetest.net/downloads/","status":"stopped"}]}

客户端 → {"type":"start_game","game":"minetest"}
服务端 → {"type":"game_status","game":"minetest","status":"running"}

客户端 → {"type":"stop_game","game":"minetest"}
服务端 → {"type":"game_status","game":"minetest","status":"stopped"}
```

### 5.6 出错时的样子

```jsonc
// 未知游戏
{ "type": "error", "message": "未知游戏: xxx" }
// 未知消息类型
{ "type": "error", "message": "未知消息类型: xxx" }
// Docker 权限不足（最常见的环境问题）
{ "type": "error",
  "message": "Minetest 状态查询失败：permission denied while trying to connect to the docker API at unix:///var/run/docker.sock" }
```

### 5.7 协议设计上的几个约定（Java 端要注意）

1. **没有请求 ID / 没有 request-response 配对**。请求和响应靠 `type` 区分，
   所以客户端必须按 `type` 分发（相当于事件驱动），不能用同步 RPC 思维。
2. **`game_status` 是异步推送**，不保证和你发的 `start_game` 一一对应
   （尤其是多客户端同时操作时），UI 应该以收到的最新状态为准。
3. **所有消息都是单条 JSON，无分帧**，不需要自己拼包。
4. 目前**没有鉴权**，连上就能操作，任何人打开页面都能停你的服务器（见第六节 P0 任务）。

---

## 六、需要做什么（任务清单，按优先级）

### P0：安全与可用性（最该先补）

- [ ] **鉴权**：目前无任何认证，任何人连上 ws 就能停服。
      方案：HTTP 登录页 + Cookie/Session，或 WebSocket 握手时校验 `?token=xxx`，
      在 `Session::onHttpRead()` / `upgradeToWebSocket()` 里加拦截。
- [ ] **配置文件外置**：游戏列表硬编码在 `GameRegistry.cpp`，
      改为读取 `games.json`，这样加游戏不用重新编译（Java 的 Jackson 处理这个很顺手）。

### P1：功能增强

- [ ] **容器资源监控**：`docker stats` 解析出 CPU / 内存，实时推给前端
- [ ] **资源限制**：启动时支持 `--cpus` / `--memory`
- [ ] **多实例**：同一个游戏开多个世界（不同端口 + 不同卷）
- [ ] **玩家在线数**：解析游戏日志或查询容器，显示当前在线人数

### P2：工程化

- [ ] 容器化部署（自己也用 Docker 跑，`docker-compose.yml`）
- [ ] CI：GitHub Actions 自动编译 + 跑 `tests/` 下的压测
- [ ] 补充单元测试

### 给 Java 同学的两条具体路线（选一个）

**路线 A（推荐，工作量适中）：用 Java 重写桌面客户端**

现在的 Qt 客户端有个现实问题：玩家要装 Qt 运行时，分发很麻烦。
用 **JavaFX + Java-WebSocket** 写一个跨平台客户端（打成 fat jar），
Win/Mac/Linux 双击就能用，比 Qt 好分发得多。

需要做的：
1. `mvn` 建工程，引依赖 `org.java-websocket:Java-WebSocket` + `jackson-databind`
2. 连 `ws://host:8889`，连上后发 `list_games` + `list_available`
3. 表格展示游戏（名称/描述/状态），每行加 启动/停止/日志/下载客户端 按钮
4. 最近 100 行日志用 `TextArea` 展示

**路线 B（工作量大，适合练手/对比）：用 Java 重写后端**

Spring Boot + `docker-java` 替代 Boost.Beast 版本，接口协议保持完全一致
（这样现有 Web 前端不用改就能直接对接），可以拿来做性能对比。

---

## 七、Java 最小可跑示例（10 分钟能跑通）

### 7.1 Maven 依赖

```xml
<dependencies>
  <dependency>
    <groupId>org.java-websocket</groupId>
    <artifactId>Java-WebSocket</artifactId>
    <version>1.5.6</version>
  </dependency>
  <dependency>
    <groupId>com.fasterxml.jackson.core</groupId>
    <artifactId>jackson-databind</artifactId>
    <version>2.17.2</version>
  </dependency>
</dependencies>
```

### 7.2 连接 + 列游戏 + 启动

```java
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.java_websocket.client.WebSocketClient;
import org.java_websocket.handshake.ServerHandshake;

import java.net.URI;

public class GameDeckClient extends WebSocketClient {

    private static final ObjectMapper MAPPER = new ObjectMapper();

    public GameDeckClient(URI uri) {
        super(uri);   // ws://192.168.220.129:8889
    }

    @Override
    public void onOpen(ServerHandshake hs) {
        System.out.println("已连接: " + hs.getHttpStatus());
        send("{\"type\":\"list_games\"}");
        send("{\"type\":\"list_available\"}");
    }

    @Override
    public void onMessage(String msg) {
        try {
            JsonNode n = MAPPER.readTree(msg);
            switch (n.path("type").asText()) {
                case "game_list" -> {
                    for (JsonNode g : n.path("games")) {
                        System.out.printf("[%s] %s -> %s (客户端: %s)%n",
                            g.path("name").asText(),
                            g.path("display").asText(),
                            g.path("status").asText(),
                            g.path("download_url").asText(""));
                    }
                    // 演示：启动 minetest
                    send("{\"type\":\"start_game\",\"game\":\"minetest\"}");
                }
                case "available_list" -> { /* 未安装游戏，可显示「安装」按钮 */ }
                case "game_status" -> System.out.println("状态变更: " + msg);
                case "error" -> System.err.println("错误: " + n.path("message").asText());
                default -> System.out.println("收到: " + msg);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override public void onClose(int code, String reason, boolean remote) {
        System.out.println("断开: " + reason);
    }

    @Override public void onError(Exception ex) { ex.printStackTrace(); }

    public static void main(String[] args) throws Exception {
        GameDeckClient c = new GameDeckClient(new URI("ws://127.0.0.1:8889"));
        c.connectBlocking();
        Thread.sleep(60_000);   // 保持连接看输出
        c.close();
    }
}
```

跑之前记得后端已 `./run.sh 8889` 启动。

---

## 八、联调与踩坑记录（都是实际踩过的）

| 现象 | 原因 | 解决 |
|---|---|---|
| `permission denied ... docker.sock` | 用户不在 docker 组，或加了组没重新登录 | `sudo usermod -aG docker $USER` 后**重新登录** |
| `bind: Address already in use` | 旧进程占着端口 | `run.sh` 已自动用 `fuser` 清理；手动可 `fuser -k 8889/tcp` |
| 浏览器打开是空白 / 握手报 `Connection field missing upgrade token` | 后端早期只做 WebSocket，不返回 HTML | 已修复：后端同端口托管 `web/` 静态文件 |
| 游戏启动成功但客户端连不上 | 游戏走 **UDP**，防火墙只放了 TCP | `sudo ufw allow 30000/udp`；云服务器要在安全组加 UDP 入站 |
| 跨机器连不上（虚拟机） | 虚拟机是 NAT 内网地址 | 用桥接模式，或在宿主机做端口转发 |
| Minetest 报找不到游戏 | 游戏内容目录/目录名不匹配 | 见 `docker/minetest/Dockerfile`，内容需放 `/usr/share/minetest/games/<gameid>` |

---

## 九、协作约定

- 分支：`main` 保持可运行，开发在 `develop`，功能分支 `feature/xxx`
- 提交信息：`feat: xxx` / `fix: xxx` / `docs: xxx`
- 改动后端请顺手补一行 `logInfo(...)`，服务日志是这个项目排查问题的主要手段
- 新增游戏 → 同步更新 `README.md` 的「特性」和 `GameRegistry.cpp`

---

## 十、还有疑问看哪里

| 想知道 | 看哪个文件 |
|---|---|
| 整体设计、并发模型 | `DESIGN.md` |
| 部署到服务器 | `DEPLOY.md` |
| 怎么参与贡献 | `CONTRIBUTING.md` |
| 消息类型定义 | `src/Protocol.h` |
| 消息分发逻辑 | `src/Session.cpp` 的 `handleMessage()` |
| Docker 操作实现 | `src/GameManager.cpp` |
| 游戏配置 | `src/GameRegistry.cpp` 构造函数 |
