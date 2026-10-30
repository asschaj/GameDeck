#include "Server.h"

#include <iostream>

#include "Session.h"

namespace gamedeck
{

Server::Server(net::io_context& ioc, unsigned short port,
               GameRegistry* registry, GameManager* manager)
    : ioc_(ioc),
      acceptor_(ioc, tcp::endpoint(tcp::v4(), port)),
      registry_(registry),
      manager_(manager)
{
}

void Server::run()
{
    std::cout << "GameDeck 服务启动，监听端口: " << acceptor_.local_endpoint().port() << std::endl;
    doAccept();
}

void Server::doAccept()
{
    acceptor_.async_accept(
        [self = shared_from_this()](boost::system::error_code ec, tcp::socket socket) {
            self->onAccept(ec, std::move(socket));
        });
}

void Server::onAccept(boost::system::error_code ec, tcp::socket socket)
{
    if (ec)
    {
        std::cerr << "接受连接失败: " << ec.message() << std::endl;
        return;
    }

    // 创建会话并运行
    std::make_shared<Session>(std::move(socket), registry_, manager_)->run();

    // 继续接受下一个连接
    doAccept();
}

}  // namespace gamedeck
