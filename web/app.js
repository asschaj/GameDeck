// GameDeck 前端逻辑：WebSocket 通信 + 游戏列表渲染 + 启停操作

let ws = null;
let reconnectTimer = null;

const connStatus = document.getElementById('conn-status');
const gameListEl = document.getElementById('game-list');
const availableListEl = document.getElementById('available-list');
const logOutput = document.getElementById('log-output');
const btnClean = document.getElementById('btn-clean');

// 连接 WebSocket（默认同源 8080 端口）
function connect() {
    const protocol = location.protocol === 'https:' ? 'wss' : 'ws';
    const host = location.hostname || '127.0.0.1';
    const port = '8080';
    ws = new WebSocket(`${protocol}://${host}:${port}`);

    ws.onopen = () => {
        connStatus.textContent = '已连接';
        connStatus.className = 'conn-badge connected';
        addLog('info', '已连接到 GameDeck 服务');
        requestGameList();
        requestAvailableList();
    };

    ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        handleMessage(msg);
    };

    ws.onclose = () => {
        connStatus.textContent = '未连接';
        connStatus.className = 'conn-badge disconnected';
        addLog('info', '连接已断开，3 秒后重连...');
        clearTimeout(reconnectTimer);
        reconnectTimer = setTimeout(connect, 3000);
    };

    ws.onerror = () => {
        addLog('error', '连接出错');
    };
}

// 处理服务端消息
function handleMessage(msg) {
    switch (msg.type) {
        case 'game_list':
            renderGameList(msg.games);
            break;
        case 'game_status':
            addLog('info', `${msg.game} 状态变为: ${msg.status}`);
            requestGameList();
            break;
        case 'clean_result':
            addLog('info', `缓存清理完成，释放 ${msg.freed_mb} MB`);
            break;
        case 'game_logs':
            addLog('info', `===== ${msg.game} 日志 =====\n${msg.logs}`);
            break;
        case 'available_list':
            renderAvailableList(msg.games);
            break;
        case 'install_result':
            if (msg.success) {
                addLog('info', `${msg.game} 安装成功: ${msg.message}`);
                // 安装成功后刷新两个列表
                requestAvailableList();
                requestGameList();
            } else {
                addLog('error', `${msg.game} 安装失败: ${msg.message}`);
            }
            break;
        case 'error':
            addLog('error', msg.message);
            break;
        default:
            addLog('info', JSON.stringify(msg));
    }
}

// 渲染游戏列表
function renderGameList(games) {
    gameListEl.innerHTML = '';
    games.forEach((game) => {
        const card = document.createElement('div');
        card.className = 'game-card';

        const title = document.createElement('h3');
        title.textContent = game.display || game.name;
        card.appendChild(title);

        const desc = document.createElement('div');
        desc.className = 'desc';
        desc.textContent = game.description || '';
        card.appendChild(desc);

        const status = document.createElement('span');
        status.className = `status ${game.status}`;
        const statusText = game.status === 'running' ? '运行中' :
                          (game.status === 'unknown' ? '状态未知' : '已停止');
        status.textContent = statusText;
        card.appendChild(status);

        const actions = document.createElement('div');
        actions.className = 'actions';

        if (game.status === 'running') {
            const stopBtn = document.createElement('button');
            stopBtn.className = 'btn btn-danger';
            stopBtn.textContent = '停止';
            stopBtn.onclick = () => send({ type: 'stop_game', game: game.name });
            actions.appendChild(stopBtn);

            const logBtn = document.createElement('button');
            logBtn.className = 'btn btn-secondary';
            logBtn.textContent = '日志';
            logBtn.onclick = () => send({ type: 'get_logs', game: game.name });
            actions.appendChild(logBtn);
        } else {
            const startBtn = document.createElement('button');
            startBtn.className = 'btn btn-primary';
            startBtn.textContent = '启动';
            startBtn.onclick = () => send({ type: 'start_game', game: game.name });
            actions.appendChild(startBtn);
        }

        card.appendChild(actions);
        gameListEl.appendChild(card);
    });
}

// 渲染可添加游戏列表
function renderAvailableList(games) {
    availableListEl.innerHTML = '';
    if (!games || games.length === 0) {
        const empty = document.createElement('div');
        empty.className = 'desc';
        empty.textContent = '暂无可添加的游戏';
        availableListEl.appendChild(empty);
        return;
    }
    games.forEach((game) => {
        const card = document.createElement('div');
        card.className = 'game-card';

        const title = document.createElement('h3');
        title.textContent = game.display || game.name;
        card.appendChild(title);

        const desc = document.createElement('div');
        desc.className = 'desc';
        desc.textContent = game.description || '';
        card.appendChild(desc);

        const actions = document.createElement('div');
        actions.className = 'actions';

        const installBtn = document.createElement('button');
        installBtn.className = 'btn btn-primary';
        installBtn.textContent = '安装';
        installBtn.onclick = () => {
            installBtn.textContent = '安装中...';
            installBtn.disabled = true;
            send({ type: 'install_game', game: game.name });
        };
        actions.appendChild(installBtn);

        card.appendChild(actions);
        availableListEl.appendChild(card);
    });
}

// 发送消息
function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(obj));
    } else {
        addLog('error', '未连接到服务');
    }
}

function requestGameList() {
    send({ type: 'list_games' });
}

function requestAvailableList() {
    send({ type: 'list_available' });
}

// 添加日志
function addLog(level, message) {
    const div = document.createElement('div');
    div.className = `log-${level}`;
    const time = new Date().toLocaleTimeString();
    div.textContent = `[${time}] ${message}`;
    logOutput.appendChild(div);
    logOutput.scrollTop = logOutput.scrollHeight;
}

// 清理缓存按钮
btnClean.onclick = () => send({ type: 'clean_cache' });

// 启动
connect();
