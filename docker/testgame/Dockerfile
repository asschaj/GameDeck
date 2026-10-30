# 本地测试游戏：不依赖网络，用于验证 install 成功路径
# 使用 busybox（如果本地有）或 scratch
FROM scratch

# scratch 是空镜像，无法运行任何命令，仅用于验证 docker build 流程
# 实际测试时可用 COPY 一个静态文件验证构建链路
COPY data.txt /data.txt

LABEL game="testgame"
