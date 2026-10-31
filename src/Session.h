#ifndef GAMEDECK_SESSION_H
#define GAMEDECK_SESSION_H

// 单个客户端会话：同时支持 HTTP 静态文件 + WebSocket
//
// 设计：
//   - 先用 beast::http 读取请求首行，判断是否为 WebSocket 升级请求
//   - 若是 Upgrade: websocket 则转为 WebSocket 会话，处理游戏管理协议
//   - 否则作为普通 HTTP GET，返回 web/ 目录下的静态文件（index.html 等）
//
// 并发设计：
//   - WebSocket 读写必须在 asio io_context 线程执行
//   - GameManager 的异步回调在工作线程执行，通过 strand 投递回 io_context

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>

#include "GameManager.h"
#include "GameRegistry.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace gamedeck
{

class Session : public std::enable_shared_from_this<Session>
{
 public:
    Session(tcp::socket socket, GameRegistry* registry, GameManager* manager,
            const std::string& webRoot);

    // 开始处理：先读 HTTP 请求，判断 WebSocket 或静态文件
    void run();

 private:
    // ===== HTTP 静态文件部分 =====
    void doReadHttp();
    void onHttpRead(beast::error_code ec, std::size_t bytes);
    void handleHttpRequest();
    void sendFile(const std::string& target);

    // ===== WebSocket 部分 =====
    void upgradeToWebSocket();
    void onAccept(beast::error_code ec);
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes);
    void doWrite(const std::string& text);
    void onWrite(beast::error_code ec, std::size_t bytes);

    // 处理客户端消息
    void handleMessage(const std::string& text);

    // 发送 JSON 消息（必须在 io_context 线程调用）
    void sendJson(const std::string& json);

    // 将结果安全投递回 io_context 线程再发送
    void postSend(const std::string& json);

    // 异步构建游戏列表并发送
    void handleListGames();

    // 返回可添加的游戏清单（同步，无需查 docker 状态）
    void handleListAvailable();

    tcp::socket socket_;          // 原始 socket（HTTP 阶段使用，升级后转交给 ws_）
    std::unique_ptr<websocket::stream<tcp::socket>> ws_;  // WebSocket 流（升级后创建）
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;  // HTTP 请求（解析首行后填充）
    bool isWebSocket_ = false;
    GameRegistry* registry_;
    GameManager* manager_;
    std::string webRoot_;         // 静态文件根目录（相对或绝对路径）
};

}  // namespace gamedeck

#endif  // GAMEDECK_SESSION_H
