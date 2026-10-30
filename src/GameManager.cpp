#include "GameManager.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>

namespace gamedeck
{

int GameManager::runCommand(const std::string& cmd, std::string* out)
{
    // 用括号包裹整个命令再重定向，确保所有子命令的 stderr 都被合并
    // （避免 `cmd1 && cmd2` 场景下 2>&1 只作用于最后一个命令）
    const std::string fullCmd = "(" + cmd + ") 2>&1";
    FILE* f = popen(fullCmd.c_str(), "r");
    if (f == nullptr)
    {
        return -1;
    }

    std::ostringstream oss;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), f) != nullptr)
    {
        oss << buffer;
    }

    int exitCode = pclose(f);
    if (out)
    {
        *out = oss.str();
    }
    return exitCode;
}

void GameManager::startGameAsync(const GameConfig& game,
                                 std::function<void(bool, const std::string&)> cb)
{
    const std::string cname = containerName(game.name);

    // 组装 docker run 命令（在工作线程中执行）
    std::string cmd;
    {
        std::ostringstream oss;
        oss << "docker run -d --name " << cname
            << " --restart unless-stopped";
        // 端口映射：每个端口一个 -p 参数
        for (const auto& port : game.ports)
        {
            oss << " -p " << port;
        }
        if (game.hasVolume && !game.volumePath.empty())
        {
            oss << " -v " << game.volumePath;
        }
        oss << " " << game.image;
        cmd = oss.str();
    }

    const std::string displayName = game.display;
    pool_->submit([this, cmd, cname, displayName, cb]() {
        // 先检查是否已在运行（检查退出码，区分命令失败）
        std::string existsOut;
        int existsRc = runCommand("docker ps -q -f name=" + cname, &existsOut);
        if (existsRc != 0)
        {
            cb(false, displayName + " 状态查询失败：" + existsOut);
            return;
        }
        if (!existsOut.empty())
        {
            cb(false, displayName + " 已在运行中");
            return;
        }

        std::string out;
        int rc = runCommand(cmd, &out);
        if (rc != 0)
        {
            cb(false, "启动失败：" + out);
            return;
        }
        cb(true, "");
    });
}

void GameManager::stopGameAsync(const GameConfig& game,
                                std::function<void(bool, const std::string&)> cb)
{
    const std::string cname = containerName(game.name);

    pool_->submit([this, cname, cb]() {
        std::string existsOut;
        int existsRc = runCommand("docker ps -q -f name=" + cname, &existsOut);
        if (existsRc != 0)
        {
            cb(false, "状态查询失败：" + existsOut);
            return;
        }
        if (existsOut.empty())
        {
            cb(false, "游戏未在运行");
            return;
        }

        std::string cmd = "docker stop " + cname + " && docker rm " + cname;
        std::string out;
        int rc = runCommand(cmd, &out);
        if (rc != 0)
        {
            cb(false, "停止失败：" + out);
            return;
        }
        cb(true, "");
    });
}

void GameManager::queryStatusAsync(const std::string& gameName,
                                   std::function<void(GameStatus)> cb)
{
    const std::string cname = containerName(gameName);

    pool_->submit([this, cname, cb]() {
        // 先查容器是否存在（检查退出码，区分命令失败和"容器不存在"）
        std::string existsOut;
        int existsRc = runCommand("docker ps -a -q -f name=" + cname, &existsOut);
        if (existsRc != 0)
        {
            // docker 命令本身失败（如权限不足），返回未知状态
            cb(GameStatus::kUnknown);
            return;
        }
        if (existsOut.empty())
        {
            cb(GameStatus::kStopped);
            return;
        }

        // 存在，查是否 running
        std::string runningOut;
        int runningRc = runCommand("docker ps -q -f name=" + cname, &runningOut);
        if (runningRc != 0)
        {
            cb(GameStatus::kUnknown);
            return;
        }
        cb(runningOut.empty() ? GameStatus::kStopped : GameStatus::kRunning);
    });
}

void GameManager::getLogsAsync(const GameConfig& game, int tailLines,
                               std::function<void(const std::string&)> cb)
{
    const std::string cname = containerName(game.name);

    pool_->submit([this, cname, tailLines, cb]() {
        std::ostringstream cmd;
        cmd << "docker logs --tail " << tailLines << " " << cname;
        std::string out;
        runCommand(cmd.str(), &out);
        cb(out);
    });
}

void GameManager::buildImageAsync(const GameConfig& game,
                                  std::function<void(bool, const std::string&)> cb)
{
    const std::string dockerfile = game.dockerfile;
    const std::string image = game.image;
    const std::string display = game.display;

    pool_->submit([this, dockerfile, image, display, cb]() {
        std::ostringstream cmd;
        cmd << "docker build -t " << image << " " << dockerfile;

        std::string out;
        int rc = runCommand(cmd.str(), &out);
        if (rc != 0)
        {
            cb(false, "镜像构建失败：" + out);
            return;
        }
        cb(true, display + " 镜像构建完成");
    });
}

void GameManager::cleanCacheAsync(std::function<void(long, const std::string&)> cb)
{
    pool_->submit([this, cb]() {
        std::string out;
        int rc = runCommand("docker container prune -f && docker image prune -f", &out);
        if (rc != 0)
        {
            cb(0, "缓存清理失败：" + out);
            return;
        }

        // 解析释放空间（docker 输出 "Total reclaimed space: 123MB"）
        long freedBytes = 0;
        size_t pos = out.find("reclaimed space");
        if (pos != std::string::npos)
        {
            size_t colon = out.find(':', pos);
            if (colon != std::string::npos)
            {
                size_t start = colon + 1;
                while (start < out.size() && (out[start] == ' ' || out[start] == '\t')) ++start;
                size_t end = start;
                while (end < out.size() && isdigit(static_cast<unsigned char>(out[end]))) ++end;
                if (end > start)
                {
                    freedBytes = std::stol(out.substr(start, end - start));
                }
            }
        }
        cb(freedBytes / (1024 * 1024), "");
    });
}

}  // namespace gamedeck
