# Mindustry 游戏服务端镜像
# Mindustry 是开源塔防游戏，服务端为单个 jar，轻量易部署
FROM eclipse-temurin:17-jre

WORKDIR /srv/mindustry

# Mindustry 服务端 jar（可替换为具体版本）
# 官方 release 下载地址：https://github.com/Anuken/Mindustry/releases
# 这里使用占位，实际构建时下载 server-release.jar
RUN apt-get update && apt-get install -y --no-install-recommends wget && \
    rm -rf /var/lib/apt/lists/*

# 下载服务端（构建时替换为实际版本号）
ARG MINDUSTRY_VERSION=146
RUN wget -q "https://github.com/Anuken/Mindustry/releases/download/v${MINDUSTRY_VERSION}/server-release.jar" -O server.jar || \
    echo "下载失败，请手动放置 server.jar"

EXPOSE 6567/tcp 6567/udp

# Mindustry 服务端默认监听 6567
CMD ["java", "-jar", "server.jar", "host"]
