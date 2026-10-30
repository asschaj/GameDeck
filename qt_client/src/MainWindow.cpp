#include "MainWindow.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPlainTextEdit>
#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

#include "GameClient.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , client_(new GameClient(this))
{
    buildUi();

    // 连接 GameClient 信号
    connect(client_, &GameClient::connected, this, &MainWindow::onConnected);
    connect(client_, &GameClient::disconnected, this, &MainWindow::onDisconnected);
    connect(client_, &GameClient::connectionError, this, [this](const QString& err) {
        appendLog("连接错误: " + err, true);
        statusLabel_->setText("连接错误");
    });
    connect(client_, &GameClient::gameListReceived, this, &MainWindow::onGameList);
    connect(client_, &GameClient::availableListReceived, this, &MainWindow::onAvailableList);
    connect(client_, &GameClient::gameLogsReceived, this, &MainWindow::onGameLogs);
    connect(client_, &GameClient::installResultReceived, this, &MainWindow::onInstallResult);
    connect(client_, &GameClient::cleanResultReceived, this, [this](int freedMb) {
        appendLog(QString("缓存清理完成，释放 %1 MB").arg(freedMb));
    });
    connect(client_, &GameClient::errorReceived, this, &MainWindow::onError);
}

void MainWindow::buildUi()
{
    setWindowTitle("GameDeck - 游戏服务器管理面板");
    resize(900, 650);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);

    // ---- 顶部连接区 ----
    auto* connBox = new QGroupBox("服务器连接", central);
    auto* connLayout = new QHBoxLayout(connBox);

    connLayout->addWidget(new QLabel("地址:"));
    hostEdit_ = new QLineEdit("127.0.0.1", connBox);
    hostEdit_->setFixedWidth(140);
    connLayout->addWidget(hostEdit_);

    connLayout->addWidget(new QLabel("端口:"));
    portSpin_ = new QSpinBox(connBox);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(8080);
    connLayout->addWidget(portSpin_);

    connectBtn_ = new QPushButton("连接", connBox);
    connLayout->addWidget(connectBtn_);

    auto* cleanBtn = new QPushButton("清理缓存", connBox);
    connLayout->addWidget(cleanBtn);

    connLayout->addStretch();
    root->addWidget(connBox);

    // ---- 已安装游戏 ----
    auto* gameBox = new QGroupBox("已安装游戏", central);
    auto* gameLayout = new QVBoxLayout(gameBox);

    gameTable_ = new QTableWidget(gameBox);
    gameTable_->setColumnCount(4);
    gameTable_->setHorizontalHeaderLabels({"游戏", "描述", "状态", "操作"});
    gameTable_->horizontalHeader()->setStretchLastSection(true);
    gameTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    gameTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    gameTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    gameTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    gameTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gameTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    gameLayout->addWidget(gameTable_);
    root->addWidget(gameBox, 3);

    // ---- 可添加游戏 ----
    auto* availBox = new QGroupBox("可添加游戏", central);
    auto* availLayout = new QVBoxLayout(availBox);

    availableTable_ = new QTableWidget(availBox);
    availableTable_->setColumnCount(3);
    availableTable_->setHorizontalHeaderLabels({"游戏", "描述", "操作"});
    availableTable_->horizontalHeader()->setStretchLastSection(true);
    availableTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    availableTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    availableTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    availableTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    availableTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    availLayout->addWidget(availableTable_);
    root->addWidget(availBox, 2);

    // ---- 日志区 ----
    auto* logBox = new QGroupBox("操作日志", central);
    auto* logLayout = new QVBoxLayout(logBox);
    logView_ = new QPlainTextEdit(logBox);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(1000);
    logLayout->addWidget(logView_);
    root->addWidget(logBox, 2);

    // ---- 状态栏 ----
    statusLabel_ = new QLabel("未连接", this);
    statusBar()->addWidget(statusLabel_);

    // ---- 信号连接 ----
    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(cleanBtn, &QPushButton::clicked, this, &MainWindow::onCleanCacheClicked);
}

void MainWindow::onConnectClicked()
{
    if (client_->isConnected())
    {
        client_->disconnectFromServer();
        return;
    }
    appendLog(QString("正在连接 %1:%2 ...").arg(hostEdit_->text()).arg(portSpin_->value()));
    client_->connectToServer(hostEdit_->text(), static_cast<quint16>(portSpin_->value()));
}

void MainWindow::onConnected()
{
    connectBtn_->setText("断开");
    statusLabel_->setText("已连接");
    appendLog("已连接到 GameDeck 服务");
}

void MainWindow::onDisconnected()
{
    connectBtn_->setText("连接");
    statusLabel_->setText("未连接");
    appendLog("连接已断开");
    gameTable_->setRowCount(0);
    availableTable_->setRowCount(0);
    gameStatus_.clear();
}

void MainWindow::onCleanCacheClicked()
{
    if (!client_->isConnected())
    {
        appendLog("未连接到服务", true);
        return;
    }
    appendLog("正在清理缓存...");
    client_->cleanCache();
}

void MainWindow::onGameList(const QJsonArray& games)
{
    gameTable_->setRowCount(0);
    gameStatus_.clear();

    for (const auto& v : games)
    {
        const QJsonObject g = v.toObject();
        const QString name = g["name"].toString();
        const QString display = g["display"].toString();
        const QString desc = g["description"].toString();
        const QString status = g["status"].toString();
        gameStatus_[name] = status;

        const int row = gameTable_->rowCount();
        gameTable_->insertRow(row);

        gameTable_->setItem(row, 0, new QTableWidgetItem(display));
        gameTable_->setItem(row, 1, new QTableWidgetItem(desc));

        const QString statusText = status == "running" ? "运行中" :
                                   (status == "unknown" ? "未知" : "已停止");
        gameTable_->setItem(row, 2, new QTableWidgetItem(statusText));

        // 操作按钮
        auto* opsWidget = new QWidget(gameTable_);
        auto* opsLayout = new QHBoxLayout(opsWidget);
        opsLayout->setContentsMargins(2, 2, 2, 2);

        if (status == "running")
        {
            auto* stopBtn = new QPushButton("停止", opsWidget);
            connect(stopBtn, &QPushButton::clicked, this, [this, name]() {
                appendLog(QString("停止游戏: %1").arg(name));
                client_->stopGame(name);
            });

            auto* logBtn = new QPushButton("日志", opsWidget);
            connect(logBtn, &QPushButton::clicked, this, [this, name]() {
                appendLog(QString("获取 %1 日志...").arg(name));
                client_->getLogs(name);
            });

            opsLayout->addWidget(stopBtn);
            opsLayout->addWidget(logBtn);
        }
        else
        {
            auto* startBtn = new QPushButton("启动", opsWidget);
            connect(startBtn, &QPushButton::clicked, this, [this, name]() {
                appendLog(QString("启动游戏: %1").arg(name));
                client_->startGame(name);
            });
            opsLayout->addWidget(startBtn);
        }

        opsWidget->setLayout(opsLayout);
        gameTable_->setCellWidget(row, 3, opsWidget);
    }
}

void MainWindow::onAvailableList(const QJsonArray& games)
{
    availableTable_->setRowCount(0);

    for (const auto& v : games)
    {
        const QJsonObject g = v.toObject();
        const QString name = g["name"].toString();
        const QString display = g["display"].toString();
        const QString desc = g["description"].toString();

        const int row = availableTable_->rowCount();
        availableTable_->insertRow(row);

        availableTable_->setItem(row, 0, new QTableWidgetItem(display));
        availableTable_->setItem(row, 1, new QTableWidgetItem(desc));

        auto* installBtn = new QPushButton("安装", availableTable_);
        connect(installBtn, &QPushButton::clicked, this, [this, name, installBtn]() {
            appendLog(QString("开始安装游戏: %1（可能需要较长时间）").arg(name));
            installBtn->setText("安装中...");
            installBtn->setEnabled(false);
            client_->installGame(name);
        });
        availableTable_->setCellWidget(row, 2, installBtn);
    }
}

void MainWindow::onGameLogs(const QString& game, const QString& logs)
{
    appendLog(QString("===== %1 日志 =====\n%2").arg(game, logs));
}

void MainWindow::onInstallResult(const QString& game, bool success, const QString& msg)
{
    if (success)
    {
        appendLog(QString("%1 安装成功: %2").arg(game, msg));
        // 刷新列表
        client_->requestGameList();
        client_->requestAvailableList();
    }
    else
    {
        appendLog(QString("%1 安装失败: %2").arg(game, msg), true);
        client_->requestAvailableList();
    }
}

void MainWindow::onError(const QString& errMsg)
{
    appendLog("错误: " + errMsg, true);
}

void MainWindow::appendLog(const QString& text, bool isError)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString line = QString("[%1] %2").arg(ts, text);
    logView_->appendPlainText(line);
    if (isError)
    {
        // 错误日志可考虑颜色标记，此处简化
    }
}
