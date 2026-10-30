# Minecraft (Paper) 游戏服务端镜像
# Paper 是高性能 Minecraft 服务端，需先同意 EULA
FROM eclipse-temurin:21-jre

WORKDIR /srv/minecraft

# 下载 Paper 服务端（构建时替换为具体版本）
ARG PAPER_VERSION=1.21
RUN apt-get update && apt-get install -y --no-install-recommends wget && \
    rm -rf /var/lib/apt/lists/*

# 创建数据目录并接受 EULA（首次运行需同意）
RUN mkdir -p /srv/minecraft && \
    echo "eula=true" > /srv/minecraft/eula.txt

EXPOSE 25565/tcp

# Paper 服务端默认监听 25565
CMD ["java", "-Xmx2G", "-jar", "paper.jar", "nogui"]
