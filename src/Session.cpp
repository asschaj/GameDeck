#include "Session.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

namespace gamedeck
{

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

Session::Session(tcp::socket socket, GameRegistry* registry, GameManager* manager)
    : ws_(std::move(socket)),
      registry_(registry),
      manager_(manager)
{
}

void Session::run()
{
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    ws_.async_accept(beast::bind_front_handler(
        &Session::onAccept, shared_from_this()));
}

void Session::onAccept(beast::error_code ec)
{
    if (ec)
    {
        std::cerr << "WebSocket 握手失败: " << ec.message() << std::endl;
        return;
    }
    doRead();
}

void Session::doRead()
{
    ws_.async_read(buffer_, beast::bind_front_handler(
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
                // 构建成功，标记为已安装
                // 注意：这里通过 self 访问 registry_，需保证线程安全（registry 内部有锁）
                self->registry_->markInstalled(gameName);
                json = "{\"type\":\"install_result\",\"game\":\"" + jsonEscape(gameName) +
                       "\",\"success\":true,\"message\":\"" + jsonEscape(msg) + "\"}";
            }
            else
            {
                json = "{\"type\":\"install_result\",\"game\":\"" + jsonEscape(gameName) +
                       "\",\"success\":false,\"message\":\"" + jsonEscape(msg) + "\"}";
            }
            self->postSend(json);
        });
    }
    else if (type == msg::kStartGame)
    {
        const std::string gameName = extractString(text, "game");
        const GameConfig* game = registry_->find(gameName);
        if (game == nullptr)
        {
            sendJson("{\"type\":\"error\",\"message\":\"未知游戏: " + jsonEscape(gameName) + "\"}");
            return;
        }

        // 异步启动，回调投递回事件循环
        GameConfig gameCopy = *game;
        manager_->startGameAsync(gameCopy, [self = shared_from_this(), gameCopy](bool ok, const std::string& err) {
            std::string json;
            if (ok)
            {
                json = "{\"type\":\"game_status\",\"game\":\"" + gameCopy.name +
                       "\",\"status\":\"running\"}";
            }
            else
            {
                json = "{\"type\":\"error\",\"message\":\"" + jsonEscape(err) + "\"}";
            }
            self->postSend(json);
        });
    }
    else if (type == msg::kStopGame)
    {
        const std::string gameName = extractString(text, "game");
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
            }
            else
            {
                json = "{\"type\":\"error\",\"message\":\"" + jsonEscape(err) + "\"}";
            }
            self->postSend(json);
        });
    }
    else if (type == msg::kGetLogs)
    {
        const std::string gameName = extractString(text, "game");
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
        manager_->cleanCacheAsync([self = shared_from_this()](long freedMb, const std::string& err) {
            std::string json;
            if (err.empty())
            {
                json = "{\"type\":\"clean_result\",\"freed_mb\":" + std::to_string(freedMb) + "}";
            }
            else
            {
                json = "{\"type\":\"error\",\"message\":\"" + jsonEscape(err) + "\"}";
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

    // 异步查询所有游戏状态，用 shared_ptr 收集结果，全部完成后汇总发送
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

        manager_->queryStatusAsync(name, [self = shared_from_this(), results, pending, mutex,
                                          idx, name, display, desc](GameStatus status) {
            std::string item = "{\"name\":\"" + jsonEscape(name) +
                               "\",\"display\":\"" + jsonEscape(display) +
                               "\",\"description\":\"" + jsonEscape(desc) +
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
                // 所有状态查询完成，汇总发送
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
            << ",\"description\":\"" << jsonEscape(g.description) << "\"}";
    }
    oss << "]}";
    sendJson(oss.str());
}

void Session::postSend(const std::string& json)
{
    // 投递回 io_context 线程执行，保证 ws_ 写操作线程安全
    // ws_.get_executor() 返回的 executor 绑定到创建 socket 时的 io_context
    auto ex = ws_.get_executor();
    net::post(ex, [self = shared_from_this(), json]() {
        self->doWrite(json);
    });
}

void Session::sendJson(const std::string& json)
{
    // 已在 io_context 线程中调用，直接写
    doWrite(json);
}

void Session::doWrite(const std::string& text)
{
    ws_.async_write(net::buffer(text), beast::bind_front_handler(
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
