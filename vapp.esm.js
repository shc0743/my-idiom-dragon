/*
main JavaScript file for Web-File-Explorer

*/


import { reportFatalError } from './error-reporter.js';

const updateLoadStat = (globalThis.ShowLoadProgress) ? globalThis.ShowLoadProgress : function () { };

globalThis.appInstance_ = {};


export function delay(timeout = 0) {
    return new Promise(resolve => setTimeout(resolve, timeout));
}


updateLoadStat('Waiting');
await new Promise(resolve => setTimeout(resolve));

import { registerResizableWidget } from './BindMove.js';
registerResizableWidget();

// break long tasks
await delay();

updateLoadStat('Loading Vue.js');
import { createApp } from 'vue';

// break long tasks
await delay();

updateLoadStat('Loading element-plus.css');
import { addCSS } from './BindMove.js';
fetch('./modules/element-plus/element-plus.css').then(v => v.text()).then(addCSS).catch(err => reportFatalError('无法加载 Element-Plus 样式表: ' + err));

// break long tasks
await delay();

updateLoadStat('Loading Vue Application');
const Vue_App = (await import('./components/App/app.js')).default;

updateLoadStat('Creating Vue application');
const app = createApp(Vue_App);
// break long tasks
await delay(10);
updateLoadStat('Loading Element-Plus');
{
    const element = await import('element-plus');
    app.use(element);
    // for (const i in element) {
    //     if (i.startsWith('El')) app.component(i, element[i]);
    // }
}
// break long tasks
await delay();
updateLoadStat('Creating app instance');
globalThis.appInstance_.app = app;
app.config.unwrapInjectedRef = true;
app.config.compilerOptions.isCustomElement = (tag) => tag.includes('-');
app.config.compilerOptions.comments = true;

// app.mount('#app');

updateLoadStat('Finding #myApp');
const myApp = document.getElementById('myApp');
console.assert(myApp); if (!myApp) throw new Error('FATAL: #myApp not found');

// break long tasks
await delay(10);

updateLoadStat('Mounting application to document');
app.mount(myApp);

// break long tasks
await delay();
updateLoadStat('Finishing');
globalThis.FinishLoad?.call(globalThis);















