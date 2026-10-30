# Minetest 游戏服务端镜像
# Minetest 是开源沙盒游戏（Minecraft 替代），C++ 编写，轻量
FROM alpine:3.19

# 安装 Minetest 服务端 + git（下载游戏内容用）
RUN apk add --no-cache minetest-server git

# 下载默认游戏 minetest_game，放到系统共享目录（minetest 5.8 查找路径）
# 目录名必须是 "minetest"，与 --gameid minetest 匹配
RUN mkdir -p /usr/share/minetest/games && \
    cd /usr/share/minetest/games && \
    git clone --depth 1 --branch 5.7.0 https://github.com/minetest/minetest_game.git minetest && \
    echo "name = minetest" >> minetest/game.conf

# 创建默认世界
RUN mkdir -p /root/.minetest/worlds/world

WORKDIR /srv/minetest

EXPOSE 30000/udp

# Minetest 服务端默认监听 UDP 30000
CMD ["minetestserver", "--gameid", "minetest"]
