#ifndef GAMEDECK_CLIENT_H
#define GAMEDECK_CLIENT_H

// WebSocket 客户端：封装与 GameDeck 后端的通信
// 复用后端已有协议（JSON 文本消息）

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

class QWebSocket;

class GameClient : public QObject
{
    Q_OBJECT

public:
    explicit GameClient(QObject* parent = nullptr);

    // 连接/断开服务器
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    // 发送命令
    void requestGameList();
    void requestAvailableList();
    void startGame(const QString& game);
    void stopGame(const QString& game);
    void getLogs(const QString& game);
    void installGame(const QString& game);
    void cleanCache();

signals:
    // 连接状态变化
    void connected();
    void disconnected();
    void connectionError(const QString& errMsg);

    // 收到游戏列表（已安装）
    void gameListReceived(const QJsonArray& games);
    // 收到可添加游戏列表
    void availableListReceived(const QJsonArray& games);
    // 收到游戏状态变化
    void gameStatusChanged(const QString& game, const QString& status);
    // 收到游戏日志
    void gameLogsReceived(const QString& game, const QString& logs);
    // 收到安装结果
    void installResultReceived(const QString& game, bool success, const QString& msg);
    // 收到清理结果
    void cleanResultReceived(int freedMb);
    // 收到错误
    void errorReceived(const QString& errMsg);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);

private:
    void handleMessage(const QJsonObject& obj);

    QWebSocket* socket_;
};

#endif // GAMEDECK_CLIENT_H
