# fix_menu.py — 重写 DrawContextMenu（修复菜单项点击失效）+ 智能重启/终端修复
import io
import re

P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

# ---------- 重写 DrawContextMenu 整个函数 ----------
new_fn = r'''void DrawContextMenu(void)
{
    static double sMenuOpenedAt = 0;
    if (!gApp.menuOpen) { sMenuOpenedAt = 0; return; }

    int itemCount = gApp.menuKind == 0 ? 9 : 3;
    float mw = 460;
    float mh = itemCount * 44 + 22;
    float mx = gApp.menuPos.x, my = gApp.menuPos.y;
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    if (mx + mw > W - 8) mx = W - 8 - mw;
    if (my + mh > H - 40) my = H - 40 - mh;
    Rectangle panel = {mx, my, mw, mh};

    /* 首帧记录打开时间；保护期后点击面板外才关闭（面板内点击交给菜单项处理） */
    if (sMenuOpenedAt == 0)
        sMenuOpenedAt = GetTime();
    int clickOutside = (GetTime() - sMenuOpenedAt > 0.25) &&
                       (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
                        IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) &&
                       !PtIn(panel);
    if (clickOutside) {
        gApp.menuOpen = 0;
        sMenuOpenedAt = 0;
        return;
    }

    /* 阴影 + 高对比容器 */
    DrawRectangleRounded((Rectangle){mx + 5, my + 6, mw, mh}, 0.05f, 8,
                         (Color){0, 0, 0, 100});
    Color menuBg = (CurrentTheme() == THEME_DARK) ? (Color){0x33, 0x30, 0x2E, 255}
                                                  : (Color){0xFF, 0xFF, 0xFF, 255};
    DrawRectangleRounded(panel, 0.05f, 8, menuBg);
    DrawRectangleRoundedLines(panel, 0.05f, 8, gPal.primary);

    float y = my + 12;
    Rectangle mr;

    if (gApp.menuKind == 0) {
        if (MenuItem(M_DETAIL, &mr, mx, &y, mw)) {
            extern void OpenProcDetail(unsigned long pid);
            OpenProcDetail((unsigned long)gApp.selectedPid);
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_EXPORT_CSV, &mr, mx, &y, mw)) {
            ExportProcessesCsv();
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_KILL, &mr, mx, &y, mw)) {
            if (gApp.selectedPid > 0) {
                bridge_kill_pid((unsigned int)gApp.selectedPid);
                MainUiRefresh();
                SetFlashMsg(N_KILLED_SEL);
            }
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_SMARTRE, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p && p->path[0]) {
                char wpath[300], wcmd[600];
                snprintf(wpath, sizeof(wpath), "%s", p->path);
                /* cmdline 首个 token 即 exe 路径，原样作为命令行（CreateProcess 会跳过首 token） */
                snprintf(wcmd, sizeof(wcmd), "%s", p->cmdline[0] ? p->cmdline : p->path);
                bridge_kill_pid(p->pid);
                Sleep(300);
                if (SysRelaunch(wpath, wcmd) == 0)
                    SetFlashMsg("已智能重启 %s", p->name);
                else
                    SetFlashMsg("智能重启失败");
                MainUiRefresh();
            }
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_AI_ANALY, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p && AiAvailable()) {
                char ports[256];
                int n = 0;
                ports[0] = 0;
                for (size_t k = 0; k < gApp.ports.count && n < 200; k++) {
                    if (gApp.ports.items[k].pid == p->pid) {
                        char one[32];
                        snprintf(one, sizeof(one), "%s%lu", n ? "," : "",
                                 (unsigned long)gApp.ports.items[k].port);
                        strcat(ports + n, one);
                        n += (int)strlen(one);
                    }
                }
                gApp.aiMode = 1;
                gApp.modal = 2;
                if (gApp.aiOutput) gApp.aiOutput[0] = 0;
                AiStartAnalyze(p, ports);
            } else if (!AiAvailable()) {
                SetFlashMsg(A_NOKILO);
            }
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_COPY_PATH, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p) SetClipboardText(p->path[0] ? p->path : "");
            SetFlashMsg(N_COPIED);
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_COPY_CMD, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p) SetClipboardText(p->cmdline[0] ? p->cmdline : "");
            SetFlashMsg(N_COPIED);
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_EXPLORER, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p && p->path[0]) SysShowInExplorer(p->path);
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_TERMINAL, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p && p->path[0]) {
                char dir[300];
                snprintf(dir, sizeof(dir), "%s", p->path);
                char *slash = strrchr(dir, '\\');
                if (slash) *slash = 0;
                SysOpenTerminal(dir);
            }
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
    } else {
        int port = gApp.menuKind == 1
                       ? (int)sPortRows[gApp.menuPortIdx].port
                       : gApp.menuRange[0];
        if (MenuItem(M_ELEVFIX, &mr, mx, &y, mw)) {
            int rc = SysElevatedFix(port);
            SetFlashMsg(rc == 0 ? N_FIX_STARTED
                                : rc == 1 ? N_FIX_CANCEL : "提权启动失败，请改用复制修复命令");
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_COPYFIX, &mr, mx, &y, mw)) {
            char fix[1024];
            SysCopyFixCommand(port, fix, sizeof(fix));
            SetClipboardText(fix);
            SetFlashMsg("已复制修复命令，请在管理员终端粘贴执行");
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
        if (MenuItem(M_OPENURL, &mr, mx, &y, mw)) {
            char url[128];
            snprintf(url, sizeof(url), "http://localhost:%d", port);
            SysOpenUrl(url);
            gApp.menuOpen = 0;
            sMenuOpenedAt = 0;
            return;
        }
    }
}
'''

m = re.search(r'void DrawContextMenu\(void\)\n\{.*?\n\}\n\n/\* ---------- 设置弹窗', t, re.S)
assert m, "DrawContextMenu block not found"
t = t[:m.start()] + new_fn + "\n/* ---------- 设置弹窗" + t[m.end():]

# ---------- MenuItem 面板内点击不关闭：由上面的 clickOutside 逻辑负责 ----------
# MenuItem 的 hit 检测已满足（PtIn + pressed）

# ---------- 智能重启/终端问题在 sys_bridge/上面已处理；确认 SysOpenTerminal 参数 ----------
with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("menu rewritten")

# ============ sys_bridge: 终端打开目标目录修复 ============
P = r"src\sys_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t2 = f.read()
old_term = '''int SysOpenTerminal(const char *dir)
{
    WCHAR wdir[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, MAX_PATH * 2) <= 0)
        return 1;
    HINSTANCE r = ShellExecuteW(NULL, L"open", L"cmd.exe", L"/k", wdir, SW_SHOWNORMAL);
    return (INT_PTR)r <= 32;
}'''
new_term = '''int SysOpenTerminal(const char *dir)
{
    WCHAR wdir[MAX_PATH * 2], params[MAX_PATH * 2 + 16];
    if (MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, MAX_PATH * 2) <= 0)
        return 1;
    swprintf(params, MAX_PATH * 2 + 16, L"/k cd /d \\"%ls\\"", wdir);
    HINSTANCE r = ShellExecuteW(NULL, L"open", L"cmd.exe", params, NULL, SW_SHOWNORMAL);
    return (INT_PTR)r <= 32;
}'''
if old_term in t2:
    t2 = t2.replace(old_term, new_term, 1)
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t2)
    print("terminal fixed")
else:
    print("terminal anchor miss")

# ============ 智能重启双重路径已在重写中修复（cmd=cmdline 原样） ============
