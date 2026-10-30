#ifndef GAMEDECK_GAME_REGISTRY_H
#define GAMEDECK_GAME_REGISTRY_H

// 游戏注册表：定义可管理的游戏及其 Docker 启动参数
//
// 概念区分：
//   - 「已安装游戏」：镜像已构建，可启动/停止（allGames）
//   - 「可添加游戏」：内置游戏目录，含 Dockerfile 但镜像未构建（availableGames）
//     用户选择后执行 install（docker build），构建完成后转为已安装

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gamedeck
{

struct GameConfig
{
    std::string name;         // 唯一标识（用于协议、容器名）
    std::string display;      // 展示名称
    std::string description;  // 描述
    std::string image;        // Docker 镜像名
    std::vector<std::string> ports;  // 端口映射列表，如 {"6567:6567/tcp", "6567:6567/udp"}
    bool hasVolume = false;   // 是否需要挂载数据卷
    std::string volumePath;   // 数据卷路径
    std::string dockerfile;   // Dockerfile 路径（相对项目根，如 "docker/mindustry"）
    bool installed = false;   // 镜像是否已构建（可启动）
};

class GameRegistry
{
 public:
    // stateFilePath：已安装游戏状态持久化文件路径（一行一个游戏名）
    explicit GameRegistry(const std::string& stateFilePath = "games.state");

    // 获取所有「已安装」的游戏（可启动/停止）
    std::vector<GameConfig> allGames() const;

    // 获取「可添加」的游戏清单（镜像未构建，可 install）
    std::vector<GameConfig> availableGames() const;

    // 按 name 查找游戏，不存在返回 nullptr
    const GameConfig* find(const std::string& name) const;

    // 标记游戏为已安装（docker build 成功后调用），并持久化到状态文件
    void markInstalled(const std::string& name);

    // 判断游戏是否已安装
    bool isInstalled(const std::string& name) const;

 private:
    // 加载状态文件（恢复已安装标记）
    void loadState();

    // 持久化当前已安装游戏到状态文件
    void saveState();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, GameConfig> games_;
    std::string stateFilePath_;
};

}  // namespace gamedeck

#endif  // GAMEDECK_GAME_REGISTRY_H
