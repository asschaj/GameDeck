#ifndef GAMEDECK_SESSION_H
#define GAMEDECK_SESSION_H

// 单个 WebSocket 客户端会话
// 处理消息收发、解析前端请求、调用 GameManager、推送状态
//
// 并发设计：
//   - WebSocket 读写必须在 asio io_context 线程执行
//   - GameManager 的异步回调在工作线程执行，通过 strand 投递回 io_context

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>

#include "GameManager.h"
#include "GameRegistry.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace gamedeck
{

class Session : public std::enable_shared_from_this<Session>
{
 public:
    Session(tcp::socket socket, GameRegistry* registry, GameManager* manager);

    // 执行 WebSocket 握手并进入读写循环
    void run();

 private:
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

    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    GameRegistry* registry_;
    GameManager* manager_;
};

}  // namespace gamedeck

#endif  // GAMEDECK_SESSION_H
