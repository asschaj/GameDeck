# Minetest 游戏服务端镜像
# Minetest 是开源沙盒游戏（Minecraft 替代），C++ 编写，轻量
FROM alpine:3.19

RUN apk add --no-cache minetest-server

WORKDIR /srv/minetest
RUN mkdir -p /root/.minetest/worlds/world

EXPOSE 30000/udp

# Minetest 服务端默认监听 UDP 30000
CMD ["minetestserver", "--gameid", "minetest"]
