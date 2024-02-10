import { registerResizableWidget } from './BindMove.js';

const
    app = document.getElementById('app'),
    myApp = document.getElementById('myApp'),
    usernameContainer = document.getElementById('usernameContainer'),
    createSessBtn = document.getElementById('createSessBtn'),
    iSess = document.getElementById('iSess'),
    appError = document.getElementById('appError'),
    frmAddSess = document.getElementById('frmAddSess'),
    addSess_sid = document.getElementById('addSess_sid'),
    sidDisp = document.getElementById('sidDisp'),
    playersView = document.getElementById('playersView'),
    endSession = document.getElementById('endSession'),
    __reserved01 = null;


registerResizableWidget();

myApp.style.width = window.innerWidth + 'px';
myApp.style.height = window.innerHeight + 'px';


fetch('username.php').then(v => v.text()).then(t => usernameContainer.innerText = (globalThis.username = t) + '@').catch(() => { });


function poperr(err) {
    appError.querySelector('pre').innerText = err;
    appError.showModal();
}



async function getSessionData() {
    return await (await fetch('session.php?action=getdata')).json();
}


createSessBtn.onclick = async () => {
    try {
        const resp = (await fetch('session.php?action=create'));
        if (resp.ok) location.reload();
        else throw `HTTP Error ${resp.status}`;
    } catch (err) {
        poperr(err);
    }
};
frmAddSess.onsubmit = (ev) => {
    ev.preventDefault();
    (async () => {
        try {
            const resp = (await fetch('session.php?action=join&sid=' + addSess_sid.value));
            if (resp.ok) location.reload();
            else throw `HTTP Error ${resp.status}`;
        } catch (err) {
            poperr(err);
        }
    })();
};



function queryPlayers()  {
    fetch('session.php?action=getplayers').then(v => v.text()).then(t => {
        playersView.innerHTML = '';
        const arr = t.split(',').filter(el => !!el.trim());
        playersView.append(document.createTextNode('在线用户：'));
        for (let i = 0, l = arr.length; i < l; ++i) {
            const el = document.createElement('div');
            el.className = 'app-player-view-box';
            el.append(document.createTextNode(arr[i]));
            if (l - i > 1) el.append(document.createTextNode(','));
            playersView.append(el);
        }
        if (arr[0] === globalThis.username) endSession.hidden = false;
    }).catch(() => { });
}
async function processSessionData() {
    try {
        const resp = await fetch('session.php?action=getdata');
        if (!resp.ok) throw resp;
    } catch (error) {
        if (error && error.status === 404) {
            clearInterval(globalThis.nIntervalQueryPlayers);
            clearInterval(globalThis.nIntervalQueryData);
            appError.onclose = () => location.reload();
            poperr("会话已结束。");
        }
    }
}
getSessionData().then(async (data) => {
    fetch('session.php?action=getid').then(v => v.text()).then(t => sidDisp.innerText = t).catch(() => { });
    iSess.hidden = false;

    globalThis.nIntervalQueryPlayers = setInterval(queryPlayers, 2500);
    queueMicrotask(queryPlayers);

    globalThis.nIntervalQueryData = setInterval(processSessionData, 1000);
    queueMicrotask(processSessionData);
}).catch(error => {
    selectSessionWizard.hidden = false;
});

endSession.onclick = () => {
    return confirm("您真的要结束此会话吗？");
};


// dlg.showModal();

