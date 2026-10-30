# Terraria (TShock) 游戏服务端镜像
# TShock 是 Terraria 的流行服务端扩展
FROM mono:latest

WORKDIR /srv/terraria

RUN apt-get update && apt-get install -y --no-install-recommends wget unzip && \
    rm -rf /var/lib/apt/lists/*

# 下载 TShock 服务端（构建时替换为具体版本）
# 官方下载：https://github.com/Pryaxis/TShock/releases
ARG TSHOCK_VERSION=5.1.3
RUN wget -q "https://github.com/Pryaxis/TShock/releases/download/v${TSHOCK_VERSION}/TShock-${TSHOCK_VERSION}.zip" -O tshock.zip || \
    echo "下载失败，请手动放置 TShock.zip"

EXPOSE 7777/tcp

# TShock 服务端默认监听 7777
CMD ["mono", "TShock.Server.exe"]
