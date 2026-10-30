#ifndef GAMEDECK_MAIN_WINDOW_H
#define GAMEDECK_MAIN_WINDOW_H

// 主窗口：游戏服务器管理面板

#include <QMainWindow>
#include <QJsonArray>
#include <QHash>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class QPlainTextEdit;
class QLabel;
class GameClient;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onConnectClicked();
    void onConnected();
    void onDisconnected();
    void onCleanCacheClicked();

    // GameClient 回调
    void onGameList(const QJsonArray& games);
    void onAvailableList(const QJsonArray& games);
    void onGameLogs(const QString& game, const QString& logs);
    void onInstallResult(const QString& game, bool success, const QString& msg);
    void onError(const QString& errMsg);

private:
    void buildUi();
    void appendLog(const QString& text, bool isError = false);

    GameClient* client_;

    // 顶部连接区
    QLineEdit* hostEdit_;
    QSpinBox* portSpin_;
    QPushButton* connectBtn_;

    // 已安装游戏表格
    QTableWidget* gameTable_;
    // 可添加游戏表格
    QTableWidget* availableTable_;

    // 日志区
    QPlainTextEdit* logView_;

    // 状态栏
    QLabel* statusLabel_;

    // 记录每个游戏当前状态（用于判断按钮显示）
    QHash<QString, QString> gameStatus_;
};

#endif // GAMEDECK_MAIN_WINDOW_H
