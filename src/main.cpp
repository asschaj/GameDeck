// GameDeck 游戏服务器托管面板 - 后端入口
//
// 功能：通过 WebSocket 提供游戏列表查询、游戏容器启动/停止、缓存清理
// 技术栈：C++17 + Boost.Beast(WebSocket) + Docker

#include <boost/asio/io_context.hpp>
#include <cstdlib>
#include <iostream>

#include "GameManager.h"
#include "GameRegistry.h"
#include "Server.h"
#include "ThreadPool.h"

namespace net = boost::asio;

int main(int argc, char* argv[])
{
    // 默认端口 8080，可通过命令行参数指定
    unsigned short port = 8080;
    if (argc > 1)
    {
        port = static_cast<unsigned short>(std::atoi(argv[1]));
    }

    try
    {
        net::io_context ioc;

        // Docker 命令线程池（执行阻塞的 docker CLI，避免阻塞事件循环）
        gamedeck::ThreadPool pool(4);

        // 业务对象
        gamedeck::GameRegistry registry;
        gamedeck::GameManager manager(&pool);

        // 创建并启动服务器
        auto server = std::make_shared<gamedeck::Server>(ioc, port, &registry, &manager);
        server->run();

        std::cout << "GameDeck 服务运行中，Docker 命令线程池: 4 线程" << std::endl;

        // 进入事件循环
        ioc.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "启动失败: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
