#include "GameClient.h"

#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

GameClient::GameClient(QObject* parent)
    : QObject(parent)
    , socket_(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    connect(socket_, &QWebSocket::connected, this, &GameClient::onConnected);
    connect(socket_, &QWebSocket::disconnected, this, &GameClient::onDisconnected);
    connect(socket_, &QWebSocket::textMessageReceived, this, &GameClient::onTextMessageReceived);
    // Qt5 中 QWebSocket 的错误信号是 QAbstractSocket::error（需 QOverload 区分重载）
    connect(socket_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, [this](QAbstractSocket::SocketError) {
        emit connectionError(socket_->errorString());
    });
}

void GameClient::connectToServer(const QString& host, quint16 port)
{
    QUrl url;
    url.setScheme("ws");
    url.setHost(host);
    url.setPort(port);
    socket_->open(url);
}

void GameClient::disconnectFromServer()
{
    socket_->close();
}

bool GameClient::isConnected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void GameClient::requestGameList()
{
    socket_->sendTextMessage("{\"type\":\"list_games\"}");
}

void GameClient::requestAvailableList()
{
    socket_->sendTextMessage("{\"type\":\"list_available\"}");
}

void GameClient::startGame(const QString& game)
{
    QJsonObject obj;
    obj["type"] = "start_game";
    obj["game"] = game;
    socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void GameClient::stopGame(const QString& game)
{
    QJsonObject obj;
    obj["type"] = "stop_game";
    obj["game"] = game;
    socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void GameClient::getLogs(const QString& game)
{
    QJsonObject obj;
    obj["type"] = "get_logs";
    obj["game"] = game;
    socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void GameClient::installGame(const QString& game)
{
    QJsonObject obj;
    obj["type"] = "install_game";
    obj["game"] = game;
    socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void GameClient::cleanCache()
{
    socket_->sendTextMessage("{\"type\":\"clean_cache\"}");
}

void GameClient::onConnected()
{
    emit connected();
    // 连接后立即请求列表
    requestGameList();
    requestAvailableList();
}

void GameClient::onDisconnected()
{
    emit disconnected();
}

void GameClient::onTextMessageReceived(const QString& message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
    {
        return;
    }
    handleMessage(doc.object());
}

void GameClient::handleMessage(const QJsonObject& obj)
{
    const QString type = obj["type"].toString();

    if (type == "game_list")
    {
        emit gameListReceived(obj["games"].toArray());
    }
    else if (type == "available_list")
    {
        emit availableListReceived(obj["games"].toArray());
    }
    else if (type == "game_status")
    {
        emit gameStatusChanged(obj["game"].toString(), obj["status"].toString());
    }
    else if (type == "game_logs")
    {
        emit gameLogsReceived(obj["game"].toString(), obj["logs"].toString());
    }
    else if (type == "install_result")
    {
        emit installResultReceived(obj["game"].toString(), obj["success"].toBool(), obj["message"].toString());
    }
    else if (type == "clean_result")
    {
        emit cleanResultReceived(obj["freed_mb"].toInt());
    }
    else if (type == "error")
    {
        emit errorReceived(obj["message"].toString());
    }
}
