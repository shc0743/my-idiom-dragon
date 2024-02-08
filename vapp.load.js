export async function onPageLoad() {

    fetch('username.php').then(v => v.text()).then(t =>
        globalThis.appInstance_.instance.$data.username =
        (globalThis.username = t)).catch(() => { });




    async function getSessionData() {
        return await (await fetch('session.php?action=getdata')).json();
    }





    function queryPlayers() {
        fetch('session.php?action=getplayers').then(v => v.text()).then(t => {
            const arr = t.split(',').filter(el => !!el.trim());
            globalThis.appInstance_.instance.$data.onlineUsers.length = 0;
            globalThis.appInstance_.instance.$data.onlineUsers.push.apply(globalThis.appInstance_.instance.$data.onlineUsers, arr)
            if (globalThis.username === arr[0]) {
                globalThis.appInstance_.instance.$data.canEndSess = true;
            }
        }).catch(() => { });
    }
    async function processSessionData() {
        try {
            const resp = await fetch('session.php?action=getdata');
            if (!resp.ok) throw resp;
        } catch (error) {
            if (error && error.status === 404) {
                clearInterval(globalThis.appInstance_.nIntervalQueryPlayers);
                clearInterval(globalThis.appInstance_.nIntervalQueryData);
                appError.onclose = () => location.reload();
                globalThis.appInstance_.instance.sessionEnded();
            }
        }
    }
    getSessionData().then(async (data) => {
        fetch('session.php?action=getid').then(v => v.text()).then(t => globalThis.appInstance_.instance.$data.sessionId = t).catch(() => { });
        globalThis.appInstance_.instance.$data.current_page = 'inProgress';

        globalThis.appInstance_.nIntervalQueryPlayers = setInterval(queryPlayers, 2500);
        queueMicrotask(queryPlayers);

        globalThis.appInstance_.nIntervalQueryData = setInterval(processSessionData, 1000);
        queueMicrotask(processSessionData);
    }).catch(error => {
        globalThis.appInstance_.instance.$data.current_page = 'new';
    });

};

