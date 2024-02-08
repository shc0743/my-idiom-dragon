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
            canEndSess: false,
            onlineUsers: [],
            userInput: '',
            addsess_value: '',

        };
    },

    components: {
        
    },

    computed: {
        htmlEl() {
            return document.querySelector(`[data-v-${componentId}]`);
        },
    },

    provide() {
        return {
            apptitle: computed(() => this.apptitle),
            
        }
    },

    methods: {
        poperr(t) { ElMessageBox.alert(t, document.title, { type: 'error' }) },
        async createSession() {
            try {
                const resp = (await fetch('session.php?action=create'));
                if (resp.ok) location.reload();
                else throw `HTTP Error ${resp.status}`;
            } catch (err) {
                this.poperr(err);
            }
        },
        async joinSession() {
            try {
                const resp = (await fetch('session.php?action=join&sid=' + this.addsess_value));
                if (resp.ok) location.reload();
                else throw `HTTP Error ${resp.status}`;
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

        },
        async leaveSess() {
            try { await ElMessageBox.confirm(
                '现在离开，您的账户可能遭到惩罚、封禁或删除！<br>确认离开吗？',
                '离开', {
                confirmButtonText: '我已知晓风险，确认离开',
                cancelButtonText: '不离开',
                type: 'error',
                draggable: true,
                dangerouslyUseHTMLString: true,
            }); } catch { return }
            try {
                const type = await (await fetch('session.php?action=checkAccountPunishType')).text();
                await ElMessageBox.confirm(
                    '这是最后一次警告！！<br>如果您现在离开，您的账户将遭到惩罚！<br>惩罚内容：' + type + "<br><br>继续操作即代表您同意该惩罚！！！<br>确认继续吗？",
                    '离开', {
                    confirmButtonText: '我自愿承担以上惩罚，确认离开',
                    cancelButtonText: '不离开',
                    type: 'error',
                    draggable: true,
                    dangerouslyUseHTMLString: true,
                });
            } catch (error) {
                ElMessage.error('操作失败：' + error);
            }
        },
        sessionEnded() {
            ElMessageBox.alert('会话已结束。', document.title, { type: 'error' })
                .then(() => location.reload());
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

    },

    template: await getHTML(import.meta.url, componentId),

};


export default data;

