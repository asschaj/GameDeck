#include "Session.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

namespace gamedeck
{

// ==================== 工具函数 ====================

// 带时间戳的日志输出（线程安全）
static void logInfo(const std::string& msg)
{
    static std::mutex logMutex;
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;

    std::lock_guard<std::mutex> lock(logMutex);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::cout << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
              << "." << std::setw(3) << std::setfill('0') << ms.count()
              << "] " << msg << std::endl;
}

// 简单的 JSON 字符串转义
static std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

// 从 JSON 字符串中提取 "key":"value" 的 value
static std::string extractString(const std::string& json, const std::string& key)
{
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";

    pos = json.find('"', pos + pattern.size());
    if (pos == std::string::npos) return "";
    size_t start = pos + 1;
    size_t end = json.find('"', start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
}

static std::string extractType(const std::string& json)
{
    return extractString(json, "type");
}

// 根据扩展名返回 Content-Type
static std::string contentTypeOf(const std::string& path)
{
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".html") == 0) return "text/html; charset=utf-8";
    if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".css") == 0)  return "text/css; charset=utf-8";
    if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".js") == 0)   return "application/javascript; charset=utf-8";
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".png") == 0)  return "image/png";
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".ico") == 0)  return "image/x-icon";
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".svg") == 0)  return "image/svg+xml";
    return "application/octet-stream";
}

// ==================== 构造与入口 ====================

Session::Session(tcp::socket socket, GameRegistry* registry, GameManager* manager,
                 const std::string& webRoot)
    : socket_(std::move(socket)),
      registry_(registry),
      manager_(manager),
      webRoot_(webRoot)
{
}

void Session::run()
{
    // 先读取 HTTP 请求，判断是否为 WebSocket 升级请求
    doReadHttp();
}

// ==================== HTTP 静态文件 ====================

void Session::doReadHttp()
{
    auto self = shared_from_this();
    http::async_read(socket_, buffer_, req_, beast::bind_front_handler(
        &Session::onHttpRead, self));
}

void Session::onHttpRead(beast::error_code ec, std::size_t /*bytes*/)
{
    if (ec == http::error::end_of_stream)
    {
        return;
    }
    if (ec)
    {
        std::cerr << "HTTP 读取失败: " << ec.message() << std::endl;
        return;
    }

    // 判断是否为 WebSocket 升级请求
    if (websocket::is_upgrade(req_))
    {
        logInfo("收到 WebSocket 升级请求");
        upgradeToWebSocket();
        return;
    }

    // 普通 HTTP 请求：返回静态文件
    logInfo("HTTP " + std::string(req_.method_string()) + " " + std::string(req_.target()));
    handleHttpRequest();
}

// 发送一个已构建好的 HTTP 响应（用 shared_ptr 持有，避免异步写期间对象析构）
static void sendResponse(tcp::socket& socket, http::request<http::string_body>& req,
                         http::status status, const std::string& body,
                         const std::string& contentType)
{
    auto res = std::make_shared<http::response<http::string_body>>(status, req.version());
    res->set(http::field::server, "GameDeck");
    res->set(http::field::content_type, contentType);
    res->keep_alive(false);
    res->body() = body;
    res->prepare_payload();

    // 捕获 res 的 shared_ptr，保证写完成前 response 存活
    http::async_write(socket, *res, [res](beast::error_code, std::size_t) {});
}

void Session::handleHttpRequest()
{
    // 只支持 GET
    if (req_.method() != http::verb::get)
    {
        sendResponse(socket_, req_, http::status::method_not_allowed,
                     "Method Not Allowed", "text/plain");
        return;
    }

    sendFile(std::string(req_.target()));
}

void Session::sendFile(const std::string& target)
{
    // 解析路径：去掉 query 参数
    std::string path = target;
    size_t q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);

    // 默认首页
    if (path == "/" || path.empty())
    {
        path = "/index.html";
    }

    // 防止路径穿越：只允许访问 webRoot 下的文件
    std::string filePath = webRoot_;
    std::string rel = path;
    if (!rel.empty() && rel[0] == '/') rel = rel.substr(1);
    if (rel.find("..") != std::string::npos)
    {
        sendResponse(socket_, req_, http::status::bad_request,
                     "Bad Request", "text/plain");
        return;
    }
    if (!filePath.empty() && filePath.back() != '/') filePath += '/';
    filePath += rel;

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open())
    {
        sendResponse(socket_, req_, http::status::not_found,
                     "<h1>404 Not Found</h1>", "text/html; charset=utf-8");
        return;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    sendResponse(socket_, req_, http::status::ok,
                 ss.str(), contentTypeOf(filePath));
}

// ==================== WebSocket ====================

void Session::upgradeToWebSocket()
{
    isWebSocket_ = true;
    ws_ = std::make_unique<websocket::stream<tcp::socket>>(std::move(socket_));
    ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    ws_->set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "GameDeck");
        }));

    // 使用已读取的请求进行握手（req_ 中已包含 Upgrade 头）
    ws_->async_accept(req_, beast::bind_front_handler(
        &Session::onAccept, shared_from_this()));
}

void Session::onAccept(beast::error_code ec)
{
    if (ec)
    {
        std::cerr << "WebSocket 握手失败: " << ec.message() << std::endl;
        return;
    }
    logInfo("WebSocket 连接建立");
    doRead();
}

void Session::doRead()
{
    ws_->async_read(buffer_, beast::bind_front_handler(
        &Session::onRead, shared_from_this()));
}

void Session::onRead(beast::error_code ec, std::size_t bytes)
{
    if (ec == websocket::error::closed)
    {
        return;
    }
    if (ec)
    {
        std::cerr << "读取失败: " << ec.message() << std::endl;
        return;
    }

    std::string text = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    handleMessage(text);
    doRead();
}

void Session::handleMessage(const std::string& text)
{
    const std::string type = extractType(text);
    logInfo("收到消息: " + type);

    if (type == msg::kListGames)
    {
        handleListGames();
    }
    else if (type == msg::kListAvailable)
    {
        handleListAvailable();
    }
    else if (type == msg::kInstallGame)
    {
        const std::string gameName = extractString(text, "game");
        logInfo("安装游戏: " + gameName);
        const GameConfig* game = registry_->find(gameName);
        if (game == nullptr)
        {
            sendJson("{\"type\":\"error\",\"message\":\"未知游戏: " + jsonEscape(gameName) + "\"}");
            return;
        }

        GameConfig gameCopy = *game;
        manager_->buildImageAsync(gameCopy, [self = shared_from_this(), gameName](bool ok, const std::string& msg) {
            std::string json;
            if (ok)
            {
                self->registry_->markInstalled(gameName);
                json = "{\"type\":\"install_result\",\"game\":\"" + jsonEscape(gameName) +
                       "\",\"success\":true,\"message\":\"" + jsonEscape(msg) + "\"}";
                logInfo("游戏安装成功: " + gameName);
            }
            else
            {
                json = "{\"type\":\"install_result\",\"game\":\"" + jsonEscape(gameName) +
                       "\",\"success\":false,\"message\":\"" + jsonEscape(msg) + "\"}";
                logInfo("游戏安装失败: " + gameName + " - " + msg);
            }
            self->postSend(json);
        });
    }
    else if (type == msg::kStartGame)
    {
        const std::string gameName = extractString(text, "game");
        logInfo("启动游戏: " + gameName);
        const GameConfig* game = registry_->find(gameName);
        if (game == nullptr)
        {
            sendJson("{\"type\":\"error\",\"message\":\"未知游戏: " + jsonEscape(gameName) + "\"}");
            return;
        }

        GameConfig gameCopy = *game;
        manager_->startGameAsync(gameCopy, [self = shared_from_this(), gameCopy](bool ok, const std::string& err) {
            std::string json;
            if (ok)
            {
                json = "{\"type\":\"game_status\",\"game\":\"" + gameCopy.name +
                       "\",\"status\":\"running\"}";
                logInfo("游戏启动成功: " + gameCopy.name);
            }
            else
            {
                json = "{\"type\":\"error\",\"message\":\"" + jsonEscape(err) + "\"}";
                logInfo("游戏启动失败: " + gameCopy.name + " - " + err);
            }
            self->postSend(json);
        });
    }
    else if (type == msg::kStopGame)
    {
        const std::string gameName = extractString(text, "game");
        logInfo("停止游戏: " + gameName);
        const GameConfig* game = registry_->find(gameName);
        if (game == nullptr)
        {
            sendJson("{\"type\":\"error\",\"message\":\"未知游戏: " + jsonEscape(gameName) + "\"}");
            return;
        }

        GameConfig gameCopy = *game;
        manager_->stopGameAsync(gameCopy, [self = shared_from_this(), gameCopy](bool ok, const std::string& err) {
            std::string json;
            if (ok)
            {
                json = "{\"type\":\"game_status\",\"game\":\"" + gameCopy.name +
                       "\",\"status\":\"stopped\"}";
                logInfo("游戏停止成功: " + gameCopy.name);
            }
            else
            {
                json = "{\"type\":\"error\",\"message\":\"" + jsonEscape(err) + "\"}";
                logInfo("游戏停止失败: " + gameCopy.name + " - " + err);
            }
            self->postSend(json);
        });
    }
    else if (type == msg::kGetLogs)
    {
        const std::string gameName = extractString(text, "game");
        logInfo("查询游戏日志: " + gameName);
        const GameConfig* game = registry_->find(gameName);
        if (game == nullptr)
        {
            sendJson("{\"type\":\"error\",\"message\":\"未知游戏: " + jsonEscape(gameName) + "\"}");
            return;
        }

        GameConfig gameCopy = *game;
        manager_->getLogsAsync(gameCopy, 100, [self = shared_from_this(), gameName](const std::string& logs) {
            std::string json = "{\"type\":\"game_logs\",\"game\":\"" + jsonEscape(gameName) +
                               "\",\"logs\":\"" + jsonEscape(logs) + "\"}";
            self->postSend(json);
        });
    }
    else if (type == msg::kCleanCache)
    {
        logInfo("清理 Docker 缓存");
        manager_->cleanCacheAsync([self = shared_from_this()](long freedMb, const std::string& err) {
            std::string json;
            if (err.empty())
            {
                json = "{\"type\":\"clean_result\",\"freed_mb\":" + std::to_string(freedMb) + "}";
                logInfo("缓存清理完成，释放 " + std::to_string(freedMb) + " MB");
            }
            else
            {
                json = "{\"type\":\"error\",\"message\":\"" + jsonEscape(err) + "\"}";
                logInfo("缓存清理失败: " + err);
            }
            self->postSend(json);
        });
    }
    else
    {
        sendJson("{\"type\":\"error\",\"message\":\"未知消息类型: " + jsonEscape(type) + "\"}");
    }
}

void Session::handleListGames()
{
    const auto games = registry_->allGames();

    if (games.empty())
    {
        sendJson("{\"type\":\"game_list\",\"games\":[]}");
        return;
    }

    auto results = std::make_shared<std::vector<std::string>>(games.size());
    auto pending = std::make_shared<int>(static_cast<int>(games.size()));
    auto mutex = std::make_shared<std::mutex>();

    for (size_t i = 0; i < games.size(); ++i)
    {
        const GameConfig g = games[i];
        const size_t idx = i;
        const std::string name = g.name;
        const std::string display = g.display;
        const std::string desc = g.description;
        const std::string downloadUrl = g.downloadUrl;

        manager_->queryStatusAsync(name, [self = shared_from_this(), results, pending, mutex,
                                          idx, name, display, desc, downloadUrl](GameStatus status) {
            std::string item = "{\"name\":\"" + jsonEscape(name) +
                               "\",\"display\":\"" + jsonEscape(display) +
                               "\",\"description\":\"" + jsonEscape(desc) +
                               "\",\"download_url\":\"" + jsonEscape(downloadUrl) +
                               "\",\"status\":\"" + statusToString(status) + "\"}";

            bool last = false;
            {
                std::lock_guard<std::mutex> lock(*mutex);
                (*results)[idx] = item;
                --(*pending);
                last = (*pending == 0);
            }

            if (last)
            {
                std::ostringstream oss;
                oss << "{\"type\":\"game_list\",\"games\":[";
                for (size_t j = 0; j < results->size(); ++j)
                {
                    if (j > 0) oss << ",";
                    oss << (*results)[j];
                }
                oss << "]}";
                self->postSend(oss.str());
            }
        });
    }
}

void Session::handleListAvailable()
{
    const auto games = registry_->availableGames();

    std::ostringstream oss;
    oss << "{\"type\":\"available_list\",\"games\":[";
    bool first = true;
    for (const auto& g : games)
    {
        if (!first) oss << ",";
        first = false;
        oss << "{\"name\":\"" << jsonEscape(g.name) << "\""
            << ",\"display\":\"" << jsonEscape(g.display) << "\""
            << ",\"description\":\"" << jsonEscape(g.description) << "\""
            << ",\"download_url\":\"" << jsonEscape(g.downloadUrl) << "\"}";
    }
    oss << "]}";
    sendJson(oss.str());
}

void Session::postSend(const std::string& json)
{
    // 投递回 io_context 线程执行，保证 ws_ 写操作线程安全
    auto ex = ws_->get_executor();
    net::post(ex, [self = shared_from_this(), json]() {
        self->doWrite(json);
    });
}

void Session::sendJson(const std::string& json)
{
    doWrite(json);
}

void Session::doWrite(const std::string& text)
{
    ws_->async_write(net::buffer(text), beast::bind_front_handler(
        &Session::onWrite, shared_from_this()));
}

void Session::onWrite(beast::error_code ec, std::size_t /*bytes*/)
{
    if (ec)
    {
        std::cerr << "写入失败: " << ec.message() << std::endl;
    }
}

}  // namespace gamedeck
