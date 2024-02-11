import { ElMessage, ElMessageBox } from 'element-plus';


globalThis.appInstance_.isRunning = false;


export async function onPageLoad() {

    // 先尝试获得登录信息
    globalThis.appInstance_.user = {};
    try {
        const resp = await fetch("/api/auth", { method: "POST" });
        if (!resp.ok) throw null;

        try {
            globalThis.appInstance_.user.data = await (await fetch('/api/me')).json();
            globalThis.appInstance_.instance.username = globalThis.appInstance_.user.data.name;

        }
        catch (error) {
            console.error(error);
            ElMessage.error("用户数据获取失败");
        }
        globalThis.appInstance_.instance.current_page = 'loading';
        StartWebConversation();
        globalThis.appInstance_.isRunning = true;
    } catch (error) {
        globalThis.appInstance_.instance.current_page = 'login';
        fetch('/api/auth/isExpired').then(v => v.text()).then(t => {
            if (t === 'yes') ElMessage.error("登录已过期，请重新登录");
        }).catch(() => { });
    }

};


let timeoutTestSentNotDone = false;


function StartWebConversation() {
    const ws = new WebSocket(location.origin.replace('http', 'ws') + '/api/v4.4/web');
    globalThis.appInstance_.ws = ws;
    let timeoutTestId = 0;
    ws.onopen = function (event) {
        console.info('[ws]', 'server connected');
        ws.send("Here's some text that the server is urgently awaiting!");

        timeoutTestId = setInterval(() => {
            if (timeoutTestSentNotDone) return;
            timeoutTestSentNotDone = true;
            ws.s({ type: 'echo', data: { type: 'timeout-test', time: new Date().getTime() } });
        }, 1000);
        queueMicrotask(InitUserInterfaceByAskingServerStateWrapper);
    };
    ws.onmessage = function (event) {
        // console.log(event.data);
        let data = event.data;
        if (!data) return;
        try { data = JSON.parse(data) } catch { return; }
        if (data.success === false && data.type === 'error') {
            console.warn('[ws]', 'received error message from server:', data);
        }

        const handlers = ws.h.get(data.type);
        if (!handlers || !(handlers instanceof Set)) {
            // console.warn('[ws]', 'undefined type', data.type, 'in', data);
            return;
        }
        let retValue = true;
        for (const handler of handlers) {
            try {
                if (!handler.call(this, ws, data)) {
                    retValue = false;
                }
                if (handler.$_once) handlers.delete(handler);
            } catch (err) {
                console.warn('[ws]', 'unhandled error while executing handler:', err);
            }
        }
        return retValue;
    };
    ws.onclose = function (event) {
        const fatal = globalThis.appInstance_.isRunning == true;
        clearInterval(timeoutTestId);
        console[fatal ? 'error' : 'info']('[ws]', 'Connection closed.');
        if (fatal) {
            ElMessageBox.alert('与服务器的连接已丢失。', '连接断开', { type: 'error', confirmButtonText: '重新连接服务器' })
                .finally(() => location.reload());
        }
    };


    ws.h = new Map();
    ws.s = function (obj) { return ws.send(JSON.stringify(obj)) };
    ws.registerHandler = function (type, func, { once = false } = {}) {
        let arr = ws.h.get(type);
        if (!(arr instanceof Set)) arr = new Set();
        if (once) func.$_once = true;
        arr.add(func);
        ws.h.set(type, arr);
    }
    ws.deleteHandler = function (type, func = null) {
        if (!func) return ws.h.delete(type);
        let arr = ws.h.get(type);
        if (!(arr instanceof Set)) arr = new Set();
        arr.delete(func);
        ws.h.set(type, arr);
    }
}


function InitUserInterfaceByAskingServerStateWrapper() {
    return InitUserInterfaceByAskingServerState().catch(e => { console.error(e); ElMessage.error('用户状态初始化失败，请尝试刷新页面\n' + e) });
}
async function InitUserInterfaceByAskingServerState() {
    const ws = globalThis.appInstance_.ws;

    ws.registerHandler('timeout-test', (ws, data) => {
        const ctime = new Date().getTime();
        const tt = ctime - data.time;
        globalThis.appInstance_.instance.networkTimeout = tt;
        timeoutTestSentNotDone = false;
    });

    ws.registerHandler('refresh-page', (ws, data) => location.reload());
    
    ws.registerHandler('receive-user-state', (ws, data) => {
        switch (data.status) {
            case 'unassigned':
                if (globalThis.appInstance_.userState == 'running') {
                    ElMessageBox.alert('对局已结束。', document.title, { type: 'error' }).finally(
                        () => globalThis.appInstance_.instance.isGameEnded = true
                    ); return;
                }
                globalThis.appInstance_.userState = 'unassigned';
                globalThis.appInstance_.instance.current_page = 'new';
                break;
            case 'running':
                if (globalThis.appInstance_.userState == 'running') break;
                if (globalThis.appInstance_.current_page == 'new') break;
                globalThis.appInstance_.userState = 'running';
                globalThis.appInstance_.instance.current_page = 'run';
                globalThis.appInstance_.instance.sessionId = data.sid;
                ws.s({ type: 'query-session-state' });
                break;
            default:
                ElMessageBox.alert("远程服务器发回了无效的响应内容。", document.title,
                    { type: 'error', dangerouslyUseHTMLString: true });
        }
    });
    ws.s({ type: "getUserState" });

    ws.registerHandler('error-ui', (ws, data) => {
        const text = data.error || data.text;
        if (data.modal) {
            ElMessageBox.alert(text, document.title, { type: 'error' })
                .finally(() => { if (data.refresh) location.reload() });
        } else {
            ElMessage.error(text);
        }
    });


    ws.registerHandler('receive-initsession', (ws, data) => {
        if (!data.success) {
            return ElMessageBox.alert(data.reason, "对局" + ((data.action === 'join') ?
                '加入' : '创建') + '失败', { type: 'error' });
        }
        ElMessage.info('正在处理...');
        appInstance_.isRunning = false;
        ws.close();
        setTimeout(() => location.reload(), 500);
    });


    ws.registerHandler('receive-session-state', (ws, data) => {
        if (false === data.success) {
            ElMessageBox.alert("操作未能成功完成。原因：" + data.reason, document.title,
                { type: 'error' });
        }
        globalThis.appInstance_.instance.sestate = data.state;
        globalThis.appInstance_.instance.semembers = data.members;
        globalThis.appInstance_.instance.sehost = data.host;
        if (data.state >= 16) {
            ws.s({ type: 'get-dragon-data' });
        }
    });


    ws.registerHandler('dragon-event', (ws, data) => {
        // console.log(data);
        globalThis.appInstance_.instance.sestate = data.state;
        globalThis.appInstance_.instance.currentDragon = data.dragon_user;
        globalThis.appInstance_.instance.currentPhrase = data.dragon_phrase;
        globalThis.appInstance_.instance.roundCount = data.round;
        globalThis.appInstance_.instance.canSkipCount = data.skipCount;
        globalThis.appInstance_.instance.isLoser = data.isLoser;
        globalThis.appInstance_.instance.losers = data.losers;
        globalThis.appInstance_.instance.appealingPhrase = data.appealingPhrase;

        if (data.completed) {
            globalThis.appInstance_.instance.isCompleted = true;
            globalThis.appInstance_.instance.winner = data.winner;
        }

        // globalThis.appInstance_.ws.s({ type: 'get-dragon-record' });
    });


    ws.registerHandler('dragon-s2c-message', (ws, data) => {
        try {
            ElMessage[data.msgtype](data.message);
        } catch (err) { console.error(error) }
    });


    ws.registerHandler('dragon-unacceptable-phrase', async (ws, data) => {
        // globalThis.appInstance_.instance.canAppeal = true;

        try { return await ElMessageBox({
            title: document.title,
            message: `抱歉，但「${data.phrase}」似乎不是成语。`,
            showCancelButton: true,
            confirmButtonText: '我知道了',
            cancelButtonText: '申诉',
            distinguishCancelAndClose: true,
        }) } catch (reason) { if (reason !== 'cancel') return; }

        try { await ElMessageBox.confirm(
            `确定将「${data.phrase}」上报为成语吗？`,
            '申诉', {
            confirmButtonText: '申诉',
            cancelButtonText: '取消',
            type: 'info',
        }); } catch { return }
        
        ws.s({
            type: 'appeal-unacceptable-phrase',
            phrase: data.phrase
        });
        ElMessage.success('请耐心等待申诉结果。');
    });


    ws.registerHandler('dragon-loser-ack', (ws, data) => {
        globalThis.appInstance_.instance.isLoser = true;
    });


    ws.registerHandler('receive-dragon-record', (ws, data) => {
        globalThis.appInstance_.instance.dragonrec = data.records;
    });


    ws.registerHandler('dragon-removed-from-session', (ws, data) => {
        ElMessageBox.alert(data.error, document.title, { type: 'error' }).finally(
            () => globalThis.appInstance_.instance.isGameEnded = true);
    });


    /*ws.registerHandler('judge-appealed-unacceptable-phrase', async (ws, data) => {
       // globalThis.appInstance_.instance.appealedPhraseToJudge = data.phrase;
                try { await ElMessageBox.confirm(
            `有人想将此短语上报为成语。${data.phrase}`,
            '这是一个成语吗？', {
            confirmButtonText: '是的',
            cancelButtonText: '不是',
            type: 'info',
        }); } catch { return }
        ws.s({
            type: 'judge-unacceptable-phrase',
            phrase: data.phrase,
        });
        ElMessage.success('感谢您的反馈。');
    });*/

    
}

