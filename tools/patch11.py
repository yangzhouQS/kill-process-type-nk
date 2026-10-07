# patch11.py — P1/P2 全量：AI会话持久化/端口联动/详情增强/白名单/状态持久化/冒烟测试
import io

# ============ 1) sys_bridge: 详情增强（特权数+父进程名），签名加 ppid ============
P = r"src\sys_bridge.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "privileges" not in t:
    t = t.replace("""typedef struct {
    unsigned long pid;
    char title[128];      /* 主窗口标题（无窗口为空） */
    unsigned long threads;
    unsigned long handles;
    char startTime[32];   /* 进程启动时间 */
} SysProcDetail;

int SysQueryProcDetail(unsigned long pid, SysProcDetail *out);""",
"""typedef struct {
    unsigned long pid;
    char title[128];      /* 主窗口标题（无窗口为空） */
    unsigned long threads;
    unsigned long handles;
    unsigned long privileges; /* 特权数（需权限，失败为 0） */
    char parentName[64];      /* 父进程名 */
    char startTime[32];       /* 进程启动时间 */
} SysProcDetail;

int SysQueryProcDetail(unsigned long pid, unsigned long ppid, SysProcDetail *out);""")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("sys.h ok")

P = r"src\sys_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "privileges" not in t:
    # 签名
    t = t.replace("int SysQueryProcDetail(unsigned long pid, SysProcDetail *out)",
                  "int SysQueryProcDetail(unsigned long pid, unsigned long ppid, SysProcDetail *out)")
    # 特权数 + 父进程名（在 CloseHandle(h) 之前插入）
    t = t.replace("""    FILETIME ftC, ftX, ftK, ftU;""",
"""    /* 特权数 */
    HANDLE tok;
    if (OpenProcessToken(h, TOKEN_QUERY, &tok)) {
        DWORD retLen = 0;
        if (GetTokenInformation(tok, TokenPrivileges, NULL, 0, &retLen) && retLen) {
            PTOKEN_PRIVILEGES tp = (PTOKEN_PRIVILEGES)malloc(retLen);
            if (tp && GetTokenInformation(tok, TokenPrivileges, tp, retLen, &retLen))
                out->privileges = tp->PrivilegeCount;
            free(tp);
        }
        CloseHandle(tok);
    }

    FILETIME ftC, ftX, ftK, ftU;""")
    # 父进程名（CloseHandle(h) 之后）
    t = t.replace("""    CloseHandle(h);

    /* 线程数 */""",
"""    CloseHandle(h);

    /* 父进程名 */
    {
        HANDLE ph = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)ppid);
        if (ph) {
            WCHAR pname[64];
            DWORD sz = 64;
            if (QueryFullProcessImageNameW(ph, 0, pname, &sz)) {
                WCHAR *base = wcsrchr(pname, L'\\\\');
                base = base ? base + 1 : pname;
                WideCharToMultiByte(CP_UTF8, 0, base, -1, out->parentName,
                                    sizeof(out->parentName), NULL, NULL);
            }
            CloseHandle(ph);
        }
    }

    /* 线程数 */""")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("sys.c ok")

# ============ 2) app_shared: 详情文本增强 + 白名单 ============
P = r"src\app_shared.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss = []

def rep(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss.append(tag)

# 详情调用传 ppid + 文本增强（特权/父名/监听端口）
rep("""    SysProcDetail d;
    SysQueryProcDetail(pid, &d);
    sDetailPid = pid;""",
"""    SysProcDetail d;
    SysQueryProcDetail(pid, (unsigned long)p->ppid, &d);
    sDetailPid = pid;

    /* 监听端口列表 */
    char portsLine[256];
    {
        int pn = 0;
        portsLine[0] = 0;
        for (size_t k = 0; k < gApp.ports.count && pn < 200; k++) {
            if (gApp.ports.items[k].pid == p->pid) {
                char one[24];
                snprintf(one, sizeof(one), "%s%lu", pn ? ", " : "",
                         (unsigned long)gApp.ports.items[k].port);
                strcat(portsLine + pn, one);
                pn += (int)strlen(one);
            }
        }
        if (!pn)
            snprintf(portsLine, sizeof(portsLine), "(无)");
    }""", "detail-query")
rep("""             "句柄数：  %lu\\n"
             "启动时间：%s\\n"
             "窗口标题：%s\\n\n"
             "可执行路径：\\n  %s\\n\\n"
             "命令行：\\n  %s\\n"
             "项目：\\n  %s",""",
"""             "句柄数：  %lu\\n"
             "特权数：  %lu\\n"
             "父进程：  %s\\n"
             "监听端口：%s\\n"
             "启动时间：%s\\n"
             "窗口标题：%s\\n\n"
             "可执行路径：\\n  %s\\n\\n"
             "命令行：\\n  %s\\n"
             "项目：\\n  %s",""", "detail-fmt")
rep("""             (unsigned long)d.threads, (unsigned long)d.handles,
             d.startTime[0] ? d.startTime : "-",
             d.title[0] ? d.title : "(无窗口)",""",
"""             (unsigned long)d.threads, (unsigned long)d.handles,
             (unsigned long)d.privileges,
             d.parentName[0] ? d.parentName : "(已退出)",
             portsLine,
             d.startTime[0] ? d.startTime : "-",
             d.title[0] ? d.title : "(无窗口)",""", "detail-args")

# 白名单：AI 清理策略执行前过滤
rep("""void AiApplyCleanStrategy(void)
{
    if (!gApp.aiOutput)
        return;
    unsigned int pids[256];
    int n = AiParseCleanJson(gApp.aiOutput, pids, 256);
    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (bridge_kill_pid(pids[i]) == 0)
            ok++;
    }
    if (n > 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), A_APPLY_DONE, ok);
        SetFlashMsg("%s", msg);
        if (gApp.balloonNotify)
            tray_notify("AI 清理策略", msg);
        RebuildViews();
    }
}""",
"""/* 系统关键进程白名单：AI 策略/批量操作永不触碰 */
static const char *sWhitelist[] = {
    "svchost.exe", "lsass.exe", "csrss.exe", "services.exe", "smss.exe",
    "wininit.exe", "winlogon.exe", "explorer.exe", "system", "registry",
    "memcompression", "dwm.exe", "fontdrvhost.exe", "wininit.exe",
};

static int IsWhitelistedName(const char *name)
{
    for (size_t i = 0; i < sizeof(sWhitelist) / sizeof(sWhitelist[0]); i++)
        if (_stricmp(name, sWhitelist[i]) == 0)
            return 1;
    return 0;
}

void AiApplyCleanStrategy(void)
{
    if (!gApp.aiOutput)
        return;
    unsigned int pids[256];
    int n = AiParseCleanJson(gApp.aiOutput, pids, 256);
    int ok = 0, skipped = 0;
    for (int i = 0; i < n; i++) {
        BridgeProc *p = NULL;
        for (size_t k = 0; k < gApp.procs.count; k++)
            if (gApp.procs.items[k].pid == pids[i]) { p = &gApp.procs.items[k]; break; }
        if (p && IsWhitelistedName(p->name)) {
            skipped++;
            continue;
        }
        if (bridge_kill_pid(pids[i]) == 0)
            ok++;
    }
    if (n > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), A_APPLY_DONE "，白名单跳过 %d", ok, skipped);
        SetFlashMsg("%s", msg);
        if (gApp.balloonNotify)
            tray_notify("AI 清理策略", msg);
        RebuildViews();
    }
}""", "whitelist")

# _stricmp 需要/string.h 已有；加 AI session 持久化（main.c 做更好，但放这里集中）
with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("app_shared MISS:", miss if miss else "none")

# ============ 3) ui_views: 端口双击联动 ============
P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
rep("""        Rectangle rowR = {x, rowY, w, ROW_H};
        if (PtIn(rowR)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                selPortRow = i;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                selPortRow = i;
                OpenContextMenu(r->isRange ? 2 : 1, GetMousePosition(), -1, i);
            }
        }""",
"""        Rectangle rowR = {x, rowY, w, ROW_H};
        if (PtIn(rowR)) {
            if (IsDoubleClickOn(rowR) && r->p) {
                /* 端口 -> 进程联动：定位并切换到进程页签 */
                gApp.selectedPid = (int)r->pid;
                gApp.curTab = TAB_ALL;
                char msg[160];
                snprintf(msg, sizeof(msg), "已定位到 %s（PID %lu），端口 %lu",
                         r->p->name, r->pid, r->port);
                SetFlashMsg("%s", msg);
                continue;
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                selPortRow = i;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                selPortRow = i;
                OpenContextMenu(r->isRange ? 2 : 1, GetMousePosition(), -1, i);
            }
        }""", "port-link")

# 4) 排序/筛选状态持久化
rep("""void UiOnColumnResize(void)
{""",
"""void ViewsSaveState(void)
{
    bridge_config_set_long("ui.sortCol", (long)sortAllCol);
    bridge_config_set_long("ui.sortDesc", (long)sortAllDesc);
    bridge_config_set_str("ui.filter", gApp.filterBuf);
}

void ViewsLoadState(void)
{
    sortAllCol = (int)bridge_config_long("ui.sortCol", 0);
    sortAllDesc = (int)bridge_config_long("ui.sortDesc", 0);
    const char *fl = bridge_config_get_str("ui.filter", "");
    if (fl && fl[0]) {
        snprintf(gApp.filterBuf, sizeof(gApp.filterBuf), "%s", fl);
        gApp.filterLen = (int)strlen(gApp.filterBuf);
    }
}

void UiOnColumnResize(void)
{""", "state-save-fn")

# bridge_config_set_str/get_str 存在于 data_bridge 吗？——不存在，需加（纯 INI 写串）
with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("ui_views MISS:", miss if miss else "none")

# ============ 4b) data_bridge: 字符串配置 API ============
P = r"src\data_bridge.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "bridge_config_get_str" not in t:
    t = t.replace("void bridge_config_set_long(const char *key, long val);",
                  "void bridge_config_set_long(const char *key, long val);\nvoid bridge_config_set_str(const char *key, const char *val);\nconst char *bridge_config_get_str(const char *key, const char *def);")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("db.h str ok")

P = r"src\data_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "bridge_config_get_str" not in t:
    impl = '''
void bridge_config_set_str(const char *key, const char *val)
{
    wchar_t wkey[128], wval[512];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 128);
    MultiByteToWideChar(CP_UTF8, 0, val, -1, wval, 512);
    ConfigSetString(wkey, wval);
}

const char *bridge_config_get_str(const char *key, const char *def)
{
    static char out[512];
    wchar_t wkey[128], wdef[512], wout[512];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 128);
    MultiByteToWideChar(CP_UTF8, 0, def, -1, wdef, 512);
    const wchar_t *r = ConfigGetString(wkey, wout, 512, wdef);
    (void)r;
    WideCharToMultiByte(CP_UTF8, 0, wout, -1, out, sizeof(out), NULL, NULL);
    return out;
}
'''
    # ConfigGetString 签名兼容：config.h 里可能是 (key, out, outLen, def)
    t = t.replace('    const wchar_t *r = ConfigGetString(wkey, wout, 512, wdef);\n    (void)r;\n',
                  '    ConfigGetString(wkey, wout, 512, wdef);\n')
    t = t + impl
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("db.c str ok")

# ============ 5) main.c: AI session 持久化 + 状态存取调用 ============
P = r"src\main.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss2 = []

def rep2(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss2.append(tag)

rep2("""    ViewsInit();
    RefreshData();""",
    """    ViewsInit();
    ViewsLoadState();
    RefreshData();""", "load-state")

rep2("""    /* 保存窗口位置 */
    Vector2 wp = GetWindowPosition();""",
    """    /* 保存会话状态 */
    ViewsSaveState();
    {
        if (gApp.aiOutput && gApp.aiOutput[0]) {
            FILE *f = fopen("ai_session.txt", "wb");
            if (f) {
                fwrite(gApp.aiOutput, 1, strlen(gApp.aiOutput), f);
                fclose(f);
            }
        } else {
            remove("ai_session.txt");
        }
    }

    /* 保存窗口位置 */
    Vector2 wp = GetWindowPosition();""", "save-session")

rep2("""    ViewsInit();
    ViewsLoadState();
    RefreshData();""",
    """    ViewsInit();
    ViewsLoadState();
    /* 恢复 AI 会话 */
    {
        FILE *f = fopen("ai_session.txt", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (n > 0 && n < 2 * 1024 * 1024) {
                gApp.aiOutputCap = (int)n + 64;
                gApp.aiOutput = (char *)malloc((size_t)gApp.aiOutputCap);
                size_t rd = fread(gApp.aiOutput, 1, (size_t)n, f);
                gApp.aiOutput[rd] = 0;
            }
            fclose(f);
        }
    }
    RefreshData();""", "load-session")

# 筛选恢复后重建视图（LoadState 后 filterLen 可能>0）——在 RefreshData 后 RebuildViews 已有
with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("main MISS:", miss2 if miss2 else "none")
