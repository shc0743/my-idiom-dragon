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


function StartWebConversation() {
    const ws = new WebSocket(location.origin.replace('http', 'ws') + '/api/v4.4/web');
    globalThis.appInstance_.ws = ws;
    let timeoutTestId = 0;
    ws.onopen = function (event) {
        console.info('[ws]', 'server connected');
        ws.send("Here's some text that the server is urgently awaiting!");

        timeoutTestId = setInterval(() => ws.s({ type: 'echo', data: { type: 'timeout-test', time: new Date().getTime() } }), 1000);
        queueMicrotask(InitUserInterfaceByAskingServerState);
    };
    ws.onmessage = function (event) {
        // console.log(event.data);
        let data = event.data;
        if (!data) return;
        try { data = JSON.parse(data) } catch { return; }
        if (data.success === false && data.type === 'error') {
            console.warn('[ws]', 'received error message from server:', data);
        }

        switch (data.type) {
            case 'timeout-test':
                {
                    const ctime = new Date().getTime();
                    const tt = ctime - data.time;
                    globalThis.appInstance_.instance.networkTimeout = tt;
                }
                break;
        
            default:
                break;
        }
    };
    ws.onclose = function (event) {
        const fatal = globalThis.appInstance_.isRunning == true;
        clearInterval(timeoutTestId);
        console[fatal ? 'error' : 'info']('[ws]', 'Connection closed.');
        if (fatal) {
            ElMessageBox.alert('与服务器的连接已丢失。', '连接断开', { type: 'error' })
                .finally(() => location.reload());
        }
    };


    ws.s = function (obj) { return ws.send(JSON.stringify(obj)) };
}


function InitUserInterfaceByAskingServerState() {
    const ws = globalThis.appInstance_.ws;

    
}

