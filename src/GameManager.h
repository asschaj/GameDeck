#ifndef GAMEDECK_GAME_MANAGER_H
#define GAMEDECK_GAME_MANAGER_H

// 游戏进程管理器：通过 Docker CLI 管理游戏容器
//
// 职责：
//   1. 启动游戏容器（docker run）
//   2. 停止并删除游戏容器（docker stop + rm）
//   3. 查询容器运行状态
//   4. 清理无用缓存/未使用镜像
//   5. 获取容器日志
//
// 并发设计：
//   - 所有 Docker 命令（阻塞 IO）通过线程池异步执行，避免阻塞 asio 事件循环
//   - 结果通过回调返回，回调在 ThreadPool 工作线程中执行，
//     调用方需自行处理线程安全（如通过 asio post 回事件循环）

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "GameRegistry.h"
#include "Protocol.h"
#include "ThreadPool.h"

namespace gamedeck
{

class GameManager
{
 public:
    explicit GameManager(ThreadPool* pool) : pool_(pool) {}

    // ---- 异步接口（结果通过回调返回，回调在工作线程执行）----

    // 启动游戏，回调参数：是否成功、错误信息
    void startGameAsync(const GameConfig& game,
                        std::function<void(bool, const std::string&)> cb);

    // 停止游戏，回调参数：是否成功、错误信息
    void stopGameAsync(const GameConfig& game,
                       std::function<void(bool, const std::string&)> cb);

    // 查询游戏状态，回调参数：状态
    void queryStatusAsync(const std::string& gameName,
                          std::function<void(GameStatus)> cb);

    // 获取日志，回调参数：日志内容
    void getLogsAsync(const GameConfig& game, int tailLines,
                      std::function<void(const std::string&)> cb);

    // 清理缓存，回调参数：释放的 MB、错误信息
    void cleanCacheAsync(std::function<void(long, const std::string&)> cb);

    // 构建游戏镜像（docker build），回调参数：是否成功、错误信息
    void buildImageAsync(const GameConfig& game,
                         std::function<void(bool, const std::string&)> cb);

    // 容器名规范：gamedeck-<game>
    static std::string containerName(const std::string& game) { return "gamedeck-" + game; }

 private:
    // 执行 shell 命令，返回退出码，输出写入 out（在工作线程中调用）
    static int runCommand(const std::string& cmd, std::string* out);

    ThreadPool* pool_;
};

}  // namespace gamedeck

#endif  // GAMEDECK_GAME_MANAGER_H
