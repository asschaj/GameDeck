#include "GameRegistry.h"

#include <fstream>
#include <sstream>

namespace gamedeck
{

GameRegistry::GameRegistry(const std::string& stateFilePath)
    : stateFilePath_(stateFilePath)
{
    // ===== 已安装游戏（镜像默认已构建，可直接启动）=====
    {
        GameConfig c;
        c.name = "mindustry";
        c.display = "Mindustry";
        c.description = "开源塔防游戏（服务端单 jar）";
        c.image = "gamedeck/mindustry:latest";
        c.ports = {"6567:6567/tcp", "6567:6567/udp"};
        c.hasVolume = true;
        c.volumePath = "mindustry-data:/srv/mindustry/config";
        c.dockerfile = "docker/mindustry";
        c.installed = true;
        games_[c.name] = c;
    }

    {
        GameConfig c;
        c.name = "minetest";
        c.display = "Minetest";
        c.description = "开源沙盒游戏（Minecraft 替代）";
        c.image = "gamedeck/minetest:latest";
        c.ports = {"30000:30000/udp"};
        c.hasVolume = true;
        c.volumePath = "minetest-data:/root/.minetest";
        c.dockerfile = "docker/minetest";
        c.installed = true;
        games_[c.name] = c;
    }

    // ===== 可添加游戏（含 Dockerfile，镜像未构建，用户可 install）=====
    {
        GameConfig c;
        c.name = "minecraft";
        c.display = "Minecraft (Paper)";
        c.description = "经典沙盒游戏（Paper 服务端）";
        c.image = "gamedeck/minecraft:latest";
        c.ports = {"25565:25565/tcp"};
        c.hasVolume = true;
        c.volumePath = "minecraft-data:/srv/minecraft";
        c.dockerfile = "docker/minecraft";
        c.installed = false;
        games_[c.name] = c;
    }

    {
        GameConfig c;
        c.name = "terraria";
        c.display = "Terraria";
        c.description = "2D 沙盒冒险游戏（TShock 服务端）";
        c.image = "gamedeck/terraria:latest";
        c.ports = {"7777:7777/tcp"};
        c.hasVolume = true;
        c.volumePath = "terraria-data:/srv/terraria";
        c.dockerfile = "docker/terraria";
        c.installed = false;
        games_[c.name] = c;
    }

    {
        GameConfig c;
        c.name = "testgame";
        c.display = "TestGame";
        c.description = "本地测试游戏（验证 install 流程）";
        c.image = "gamedeck/testgame:latest";
        c.dockerfile = "docker/testgame";
        c.installed = false;
        games_[c.name] = c;
    }

    // 加载持久化状态：恢复之前 install 过的游戏
    loadState();
}

std::vector<GameConfig> GameRegistry::allGames() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<GameConfig> result;
    for (const auto& kv : games_)
    {
        if (kv.second.installed)
        {
            result.push_back(kv.second);
        }
    }
    return result;
}

std::vector<GameConfig> GameRegistry::availableGames() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<GameConfig> result;
    for (const auto& kv : games_)
    {
        if (!kv.second.installed)
        {
            result.push_back(kv.second);
        }
    }
    return result;
}

const GameConfig* GameRegistry::find(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = games_.find(name);
    return it == games_.end() ? nullptr : &it->second;
}

void GameRegistry::markInstalled(const std::string& name)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = games_.find(name);
        if (it == games_.end())
        {
            return;
        }
        it->second.installed = true;
    }
    // 锁外持久化，避免持锁做 IO
    saveState();
}

bool GameRegistry::isInstalled(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = games_.find(name);
    return it != games_.end() && it->second.installed;
}

void GameRegistry::loadState()
{
    std::ifstream in(stateFilePath_);
    if (!in.is_open())
    {
        return;  // 状态文件不存在，保持默认
    }

    std::string line;
    std::lock_guard<std::mutex> lock(mutex_);
    while (std::getline(in, line))
    {
        // 去掉行尾空白
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
        {
            line.pop_back();
        }
        if (line.empty()) continue;

        auto it = games_.find(line);
        if (it != games_.end())
        {
            it->second.installed = true;
        }
    }
}

void GameRegistry::saveState()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(stateFilePath_, std::ios::trunc);
    if (!out.is_open())
    {
        return;
    }
    for (const auto& kv : games_)
    {
        if (kv.second.installed)
        {
            out << kv.second.name << "\n";
        }
    }
}

}  // namespace gamedeck
