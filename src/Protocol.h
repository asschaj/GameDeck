#ifndef GAMEDECK_PROTOCOL_H
#define GAMEDECK_PROTOCOL_H

// 前后端 WebSocket 通信协议定义（JSON 文本消息）
//
// 客户端 → 服务端：
//   {"type":"list_games"}
//   {"type":"start_game","game":"mindustry"}
//   {"type":"stop_game","game":"mindustry"}
//   {"type":"clean_cache"}
//
// 服务端 → 客户端：
//   {"type":"game_list","games":[...]}
//   {"type":"game_status","game":"...","status":"..."}
//   {"type":"log","level":"...","message":"..."}
//   {"type":"clean_result","freed_mb":123}

#include <string>

namespace gamedeck
{

// 消息类型常量
namespace msg
{
inline constexpr const char* kListGames    = "list_games";
inline constexpr const char* kListAvailable = "list_available";
inline constexpr const char* kStartGame    = "start_game";
inline constexpr const char* kStopGame     = "stop_game";
inline constexpr const char* kCleanCache   = "clean_cache";
inline constexpr const char* kGetLogs      = "get_logs";
inline constexpr const char* kInstallGame  = "install_game";

inline constexpr const char* kGameList      = "game_list";
inline constexpr const char* kAvailableList = "available_list";
inline constexpr const char* kGameStatus    = "game_status";
inline constexpr const char* kLog           = "log";
inline constexpr const char* kGameLogs      = "game_logs";
inline constexpr const char* kCleanResult   = "clean_result";
inline constexpr const char* kInstallResult = "install_result";
inline constexpr const char* kError         = "error";
}  // namespace msg

// 游戏状态
enum class GameStatus
{
    kStopped,
    kRunning,
    kUnknown,
};

inline std::string statusToString(GameStatus s)
{
    switch (s)
    {
        case GameStatus::kStopped: return "stopped";
        case GameStatus::kRunning: return "running";
        default:                   return "unknown";
    }
}

inline GameStatus statusFromString(const std::string& s)
{
    if (s == "running") return GameStatus::kRunning;
    if (s == "stopped") return GameStatus::kStopped;
    return GameStatus::kUnknown;
}

}  // namespace gamedeck

#endif  // GAMEDECK_PROTOCOL_H
