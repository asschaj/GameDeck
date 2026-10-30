# GameDeck 部署与推送指南

> 本文档记录如何将本项目推送到 GitHub，以及在服务器上的部署步骤。

---

## 一、推送到 GitHub（首次）

### 前置条件

- GitHub 账号已创建（当前身份：`asschaj`）
- 网络可访问 github.com（需要梯子/代理时先开启）

### 步骤

1. **在 GitHub 网页创建空仓库**
   - 访问 https://github.com/new
   - Repository name: `GameDeck`
   - **不要**勾选 "Add a README"（避免与本地冲突）
   - **不要**勾选 license / .gitignore（本地已有）
   - 点击 Create repository

2. **本地关联远程并推送**

```bash
cd /data/home/sch/ReactorGate-OpenSource/gamedeck

# 关联远程仓库
git remote add origin https://github.com/asschaj/GameDeck.git

# 推送（首次用 -u 建立跟踪）
git push -u origin main
```

3. **如果提示认证**：GitHub 已不支持密码推送，需用 Personal Access Token：
   - GitHub → Settings → Developer settings → Personal access tokens → Generate new token
   - 勾选 `repo` 权限，生成后复制
   - 推送时密码处粘贴 token

### 常见问题

| 问题 | 解决 |
|---|---|
| `Failed to connect` | 网络不通，需开代理；或配置 git 代理：`git config --global http.proxy http://127.0.0.1:7890` |
| `Authentication failed` | 用 token 而非密码 |
| 远程已有 README 导致冲突 | `git pull origin main --allow-unrelated-histories` 后再推 |

---

## 二、服务器部署（其他人使用本项目）

### 环境要求

- Linux（Ubuntu/Debian）
- CMake ≥ 3.16、g++（C++17）
- Boost 开发库
- Docker（且当前用户有 docker 权限）

```bash
# 安装依赖（Ubuntu/Debian）
sudo apt install build-essential cmake libboost-all-dev docker.io
sudo usermod -aG docker $USER  # 重新登录生效
```

### 构建与启动

```bash
git clone https://github.com/asschaj/GameDeck.git
cd GameDeck

# 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j4

# 启动（需要 docker 权限，建议 root 或 docker 组用户）
./build/gamedeck_server 8080
```

### 游戏镜像

```bash
# 默认已安装的游戏（mindustry / minetest）需要先构建镜像
docker build -t gamedeck/mindustry:latest docker/mindustry/
docker build -t gamedeck/minetest:latest docker/minetest/

# 可添加游戏（minecraft / terraria / testgame）通过前端「安装」按钮构建
```

### 访问

浏览器打开 `http://<服务器IP>:8080`，即可看到游戏面板。

---

## 三、日常更新推送

```bash
git add -A
git commit -m "feat: 描述你的改动"
git push
```
