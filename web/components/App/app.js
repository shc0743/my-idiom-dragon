import { getHTML } from 'util/browser_side-compiler.js';
import { computed, defineAsyncComponent } from 'vue';
import { ElMessage, ElMessageBox } from 'element-plus';


const componentId = '8c2f171854a04319b6cdaaf303bd97ef';
export { componentId };
    
    
import { onPageLoad } from '@/vapp.load.js';



const data = {
    data() {
        return {
            current_page: 'unknown',
            apptitle: '',
            username: '',
            sessionId: '',
            userInput: '',
            userInput_beg: '',
            addsess_value: '',
            login_form: {},
            reg_form: {},
            chpswd: {},
            isLogging: false,
            networkTimeout: -1,
            sestate: 0,
            semembers: [],
            sehost: '',
            IsGameStarting: false,
            currentDragon: '',
            currentPhrase: '',
            lastUserPhrase: '',
            // canAppeal: false,
            roundCount: 0,
            canSkipCount: 0,
            isLoser: false,
            dragonrec: [],
            isGameEnded: false,
            // appealedPhraseToJudge: '',
            isCompleted: false,
            winner: '',
            appealingPhrase: '',
            losers: [],
            userInput_noRepeat: false,
            challengeAgain: false,
            challengeAgain_agreedMembers: null,
            challengeAgain_imagreed: false,
            challengeAgain_tids: {},

        };
    },

    components: {
        
    },

    computed: {
        htmlEl() {
            return document.querySelector(`[data-v-${componentId}]`);
        },
        networkTimeout_style() {
            return {
                color: 
                    (this.networkTimeout <= 100) ? 'green' :
                    ((this.networkTimeout <= 200) ? '#ceb36a' : '#d31111'),
                fontFamily: 'inherit'
            };
        },
        semembersHeight() {
            return window.innerHeight - 100;
        },
        semembersComputed() {
            if (!this.semembers) return [];
            return this.semembers.map(v => ({ name: v, isLoser: this.losers.includes(v) }));
        },
        dragonrecComputed() {
            if (!this.dragonrec) return [];
            return this.dragonrec.map((v, i) => ({ n: i + 1, phrase: v }));
        },
        appealingPhraseDlg: {
            get() {
                return !!this.appealingPhrase;
            },
            set(val) { this.appealingPhrase = '' }
        },
        isHost() {
            return this.sehost === this.username;  
        },
        challengeAgain_listMembers() {
            if (!this.semembers) return [];
            if (!this.challengeAgain_agreedMembers) return [];
            return this.semembers.map(v => ({ name: v, isAgreed: this.challengeAgain_agreedMembers.includes(v) }));
        },
    },

    provide() {
        return {
            apptitle: computed(() => this.apptitle),
            
        }
    },

    methods: {
        poperr(t) { ElMessageBox.alert(t, document.title, { type: 'error' }) },
        sememctl(b) { this.$refs.sememdlg[b ? 'showModal' : 'close']() },
        reloadPage() { location.reload() },
        clg() { console.log.apply(console, arguments) },
        async uLogin() {
            this.isLogging = true;

            try {
                const u = this.login_form.name, p = SHA256(this.login_form.pswd);
                const fd = new URLSearchParams;
                fd.append('user', u); fd.append('password', p);
                if (this.login_form.remember) fd.append('remember', 'true');
                const login_result = await fetch('/api/auth/login', {
                    method: 'POST',
                    body: fd,
                    headers: {
                        'content-type': 'application/x-www-form-urlencoded'
                    }
                });
                if (login_result.ok) {
                    ElMessageBox.alert('登录成功。', '登录', { type: 'success' }).finally(
                        () => location.reload()
                    );
                }
                else if (login_result.status === 401) throw '账号或密码错误';
                else throw `HTTP 错误: ${login_result.status} - ${login_result.statusText}`
            } catch (error) {
                ElMessage.error('登录失败: ' + error);
                this.isLogging = false;
            }
        },
        async uRegister() {
            this.isLogging = true;

            try {
                const u = this.reg_form.name, p = SHA256(this.reg_form.pswd);
                const fd = new URLSearchParams;
                fd.append('user', u); fd.append('password', p);
                const login_result = await fetch('/api/auth/register', {
                    method: 'POST',
                    body: fd,
                    headers: {
                        'content-type': 'application/x-www-form-urlencoded'
                    }
                });
                if (login_result.ok) {
                    this.isLogging = false;
                    ElMessageBox.alert('注册成功。', '注册', { type: 'success', confirmButtonText: '去登录' }).finally(
                        () => this.current_page = 'login'
                    );
                }
                else throw `HTTP 错误: ${login_result.status} - ${login_result.statusText}: ${await login_result.text()}`
            } catch (error) {
                ElMessage.error('注册失败: ' + error);
                this.isLogging = false;
            }
        },
        async changePswd() {
            if (this.chpswd.pswd2 !== this.chpswd.pswd3) {
                return ElMessageBox.alert('密码不一致。', '更改密码', { type: 'error' }).finally(
                    () => this.chpswd.pswd3 = ''
                );
            }
            this.isLogging = true;

            try {
                const old = SHA256(this.chpswd.pswd1), p = SHA256(this.chpswd.pswd2);
                const fd = new URLSearchParams;
                fd.append('o', old); fd.append('n', p);
                const login_result = await fetch('/api/auth/type/password/edit', {
                    method: 'POST',
                    body: fd,
                    headers: {
                        'content-type': 'application/x-www-form-urlencoded'
                    }
                });
                if (login_result.ok) {
                    this.isLogging = false;
                    ElMessageBox.alert('更改成功。', '更改密码', { type: 'success' }).finally(
                        () => location.reload()
                    );
                }
                else if (login_result.status === 403) throw '原密码错误';
                else if (login_result.status === 400) throw '提供的注册信息无效。';
                else throw `HTTP 错误: ${login_result.status} - ${login_result.statusText}`
            } catch (error) {
                ElMessage.error('更改失败: ' + error);
                this.isLogging = false;
            }
        },
        async logout() {
            fetch('/api/auth/logout', { method: 'POST' }).then(v => {
                if (!v.ok) throw v.status + ' ' + v.statusText;
                return v.text();
            }).then(() => location.reload()).catch(error => {
                ElMessageBox.alert(error, '无法退出登录', { type: 'error' }); 
            });
        },
        async createOrJoinSession(is_join) {
            try {
                appInstance_.ws.s({
                    type: 'create-or-join-session',
                    action: is_join ? 'join' : 'create',
                    target: is_join ? this.addsess_value : undefined,
                });
                ElMessage.info('正在创建或加入...');
            } catch (err) {
                this.poperr(err);
            }
        },
        async copySid() {
            try {
                await navigator.clipboard.writeText(this.sessionId)
                ElMessage.success('已复制');
            } catch (error) {
                ElMessageBox.prompt('复制失败，请手动复制', document.title, { type: 'error', inputValue: this.sessionId }) 
            }
        },
        async endSess() {
            try { await ElMessageBox.confirm(
                '如果您结束此对局，所有正在进行该对局的用户将会被请离。<br>确定继续吗？',
                '结束', {
                confirmButtonText: '继续结束',
                cancelButtonText: '不结束',
                type: 'warning',
                draggable: true,
                dangerouslyUseHTMLString: true,
            }); } catch { return }
            appInstance_.ws.s({
                type: 'leave-session',
                sid: this.sessionId,
            });
            ElMessage.info('正在处理...');
        },
        async leaveSessByUserIndex(index) {
            this.sememctl(false);
            const un = this.semembers[index];
            try { await ElMessageBox.confirm(
                '是否请离用户: ' + un,
                '请离', {
                confirmButtonText: '请离',
                cancelButtonText: '不请离',
                type: 'warning',
            }); } catch { return this.sememctl(true) }
            appInstance_.ws.s({
                type: 'kick-user-in-session',
                sid: this.sessionId,
                user: un,
            });
            ElMessage.info('正在处理...');
        },
        async leaveSess(force) {
            if (this.sestate === 100) force = true;
            try { if (!this.isLoser && !force) await ElMessageBox.confirm(
                '现在离开，您的账户可能遭到惩罚、封禁或删除！<br>确认离开吗？',
                '离开', {
                confirmButtonText: '我已知晓风险，确认离开',
                cancelButtonText: '不离开',
                type: 'error',
                draggable: true,
                dangerouslyUseHTMLString: true,
            }); } catch { return }
            try {
                if (!this.isLoser && !force) {
                    const type = await (await fetch('/api/account-punish/query')).text();
                    await ElMessageBox.confirm(
                        '这是最后一次警告！！<br>如果您现在离开，您的账户将遭到惩罚！<br>惩罚内容：' +
                        type + "<br><br>继续操作即代表您同意该惩罚！！！<br>确认继续吗？",
                        '离开', {
                        confirmButtonText: '我自愿承担以上惩罚，确认离开',
                        cancelButtonText: '不离开',
                        type: 'error',
                        draggable: true,
                        dangerouslyUseHTMLString: true,
                    });
                }
                appInstance_.ws.s({
                    type: 'leave-session',
                    sid: this.sessionId,
                });
                ElMessage.info('正在处理...');
            } catch (error) {
                ElMessage.error('操作失败：' + error);
            }
        },
        sessionEnded() {
            this.isGameEnded = true;
            ElMessageBox.alert('会话已结束。', document.title, { type: 'error' })
                .catch(() => {});
        },
        async StartGame(period) {
            if (period !== 2) try { await ElMessageBox.confirm(
                '请确认所有成员都已完成加入。在进入下一步后，其他人将无法加入此对局。', document.title, {
                confirmButtonText: '进入下一步',
                cancelButtonText: '继续等待',
                type: 'info',
            }); } catch { return }
            globalThis.appInstance_.ws.s({
                type: 'start-game',
                param: period === 2 ? this.userInput_beg : undefined,
                noRepeat: this.userInput_noRepeat ? true : undefined,
            });
            // this.IsGameStarting = true;
            ElMessage.info("正在处理...");
        },
        async SubmitDragon() {
            this.lastUserPhrase = this.userInput;
            globalThis.appInstance_.ws.s({
                type: 'submit-dragon',
                "userinput": this.userInput,
            });
            this.userInput = '';
        },
        /*async submitAppeal() {
            try { await ElMessageBox.confirm(
                `确定将「${this.lastUserPhrase}」上报为成语吗？`,
                '申诉', {
                confirmButtonText: '申诉',
                cancelButtonText: '取消',
                type: 'info',
            }); } catch { return }

        },*/
        async imLoser() {
            try { await ElMessageBox.confirm(
                '真的要认输了吗？' + ((this.canSkipCount > 0) ?
                    (`你还有${this.canSkipCount}次机会可以跳过！`) : ''), '认输', {
                confirmButtonText: '我认怂了（；´д｀）ゞ',
                cancelButtonText: '暂不认输',
                type: 'warning',
                draggable: true,
            }); } catch { return }
            globalThis.appInstance_.ws.s({ type: 'im-loser' });
            this.userInput = '';
        },
        /*async getTip() {
            globalThis.appInstance_.ws.s({
                type: 'submit-dragon',
                "userinput": this.userInput,
            });
            this.userInput = '';
        },*/
        async getSkip() {
            try { await ElMessageBox.confirm(
                '确定要跳过此回合吗?你还有' + this.canSkipCount + '次机会。', '跳过', {
                confirmButtonText: '跳过',
                cancelButtonText: '不跳过',
                type: 'warning',
                draggable: true,
            }); } catch { return }
            globalThis.appInstance_.ws.s({
                type: 'submit-dragon',
                useSkipChance: true,
            });
            this.userInput = '';
        },
        viewRecord() {
            globalThis.appInstance_.ws.s({ type: 'get-dragon-record' });
            this.$refs.dragonRecord.showModal();
        },
        async submitMyAppeal(val) {
            globalThis.appInstance_.ws.s({
                type: 'judge-unacceptable-phrase',
                result: !!val,
            });
            ElMessage.success('感谢您的反馈。');
            this.appealingPhrase = '';
        },
        async startWithSameMember() {
            globalThis.appInstance_.ws.s({
                type: 'start-with-same-members'
            });
            ElMessage.info('请稍候...');
        },
        submitChallAgainResp(agree) {
            globalThis.appInstance_.ws.s({
                type: 'start-with-same-members-resp',
                agree,
            });
            if (agree) this.challengeAgain_imagreed = true;
            else this.challengeAgain = false;
        },

    },

    created() {
        globalThis.appInstance_.instance = this;
    },

    mounted() {
        
        this.$refs.box.style.width = window.innerWidth + 'px';
        this.$refs.box.style.height = window.innerHeight + 'px';

        onPageLoad.apply(this);

    },

    watch: {
        current_page() {
            
        },
        apptitle() {
            globalThis.document.title = this.apptitle;
        },
        // currentDragon() { this.canAppeal = false },
        // currentPhrase() { this.canAppeal = false },
        roundCount() {
            if (this.isLoser) globalThis.appInstance_.ws.s({ type: 'get-dragon-record' });
        },
        challengeAgain() {
            this.challengeAgain_imagreed = false;
            if (this.challengeAgain) {
                if (this.challengeAgain_tids[0]) clearTimeout(this.challengeAgain_tids[0]);
                this.challengeAgain_tids[0] = setTimeout(() => this.challengeAgain = false, 10000);
            }
        },
        challengeAgain_agreedMembers() {
            if (this.challengeAgain && !this.challengeAgain_agreedMembers) {
                this.challengeAgain_imagreed = false;
                this.challengeAgain = false;
            }
            if (!this.isHost) return;
            this.$refs.challengeAgain_agreedMembers_dlg[this.challengeAgain_agreedMembers ? 'showModal' : 'close']();
            if (this.challengeAgain_agreedMembers) {
                if (this.challengeAgain_tids[1]) clearTimeout(this.challengeAgain_tids[1]);
                this.challengeAgain_tids[1] = setTimeout(() => {
                    this.challengeAgain_agreedMembers = null;
                    // ElMessage.error('操作超时');
                }, 10000);
            }
        },

    },

    template: await getHTML(import.meta.url, componentId),

};


export default data;

