#ifndef GAMEDECK_SERVER_H
#define GAMEDECK_SERVER_H

// WebSocket 服务器：监听端口，接受连接，创建会话

#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>

#include "GameManager.h"
#include "GameRegistry.h"

namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace gamedeck
{

class Server : public std::enable_shared_from_this<Server>
{
 public:
    Server(net::io_context& ioc, unsigned short port,
           GameRegistry* registry, GameManager* manager,
           const std::string& webRoot);

    // 启动监听
    void run();

 private:
    void doAccept();
    void onAccept(boost::system::error_code ec, tcp::socket socket);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    GameRegistry* registry_;
    GameManager* manager_;
    std::string webRoot_;
};

}  // namespace gamedeck

#endif  // GAMEDECK_SERVER_H
