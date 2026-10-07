/* tray_bridge.c — Win32 托盘实现（隔离 Win32 头文件，避免与 raylib 冲突） */
#define NOGDI
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#include "tray_bridge.h"
#include "version.h"
#include "icon_ico.h"
#include "sys_bridge.h"

#define WM_APP_TRAY (WM_APP + 1)
#define IDM_TRAY_SHOW    2001
#define IDM_TRAY_EXIT    2002
#define IDM_TRAY_REFRESH 2003
#define IDM_TRAY_KILL_NODE 2004
#define IDM_TRAY_KILL_PY   2005
#define IDM_TRAY_ORPHAN  2006
#define IDM_TRAY_SETTINGS 2007
#define IDM_TRAY_AUTORUN 2008
#define IDM_TRAY_AI_CLEAN 2009
#define IDM_TRAY_BASE_SAVE 2010
#define IDM_TRAY_BASE_CMP 2011

static NOTIFYICONDATAW s_nid;
static HWND s_hMsgWnd;
static BOOL s_added = FALSE;

static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_APP_TRAY) {
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);
            {
                HMENU m = CreatePopupMenu();
                AppendMenuW(m, MF_STRING, IDM_TRAY_REFRESH, L"刷新");
                AppendMenuW(m, MF_STRING, IDM_TRAY_KILL_NODE, L"杀死全部 Node.js");
                AppendMenuW(m, MF_STRING, IDM_TRAY_KILL_PY, L"杀死全部 Python");
                AppendMenuW(m, MF_STRING, IDM_TRAY_ORPHAN, L"清理孤儿进程");
                AppendMenuW(m, MF_STRING, IDM_TRAY_AI_CLEAN, L"AI 清理策略...");
                AppendMenuW(m, MF_STRING, IDM_TRAY_BASE_SAVE, L"保存基线快照");
                AppendMenuW(m, MF_STRING, IDM_TRAY_BASE_CMP, L"与基线对比...");
                AppendMenuW(m, MF_STRING | (SysIsAutoRun() ? MF_CHECKED : 0), IDM_TRAY_AUTORUN, L"开机自启动");
                AppendMenuW(m, MF_STRING, IDM_TRAY_SETTINGS, L"设置...");
                AppendMenuW(m, MF_SEPARATOR, 0, NULL);
                AppendMenuW(m, MF_STRING, IDM_TRAY_SHOW, L"显示 / 隐藏主界面");
                AppendMenuW(m, MF_STRING, IDM_TRAY_EXIT, L"退出");
                SetForegroundWindow(hwnd);
                TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                PostMessage(hwnd, WM_NULL, 0, 0);
                DestroyMenu(m);
            }
        } else if (lp == NIN_BALLOONUSERCLICK) {        PostMessage(hwnd, WM_COMMAND, IDM_TRAY_SHOW, 0);    } else if (lp == WM_LBUTTONUP) {
            PostMessage(hwnd, WM_COMMAND, IDM_TRAY_SHOW, 0);
        }
    } else if (msg == WM_COMMAND) {
        /* 存储待处理命令（tray_poll 消费） */
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)LOWORD(wp));
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static WNDPROC s_prevMainProc = NULL;
static HWND s_hMainWnd = NULL;

static LRESULT CALLBACK MainWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_CLOSE) {
        /* 点 X：隐藏到托盘（真正退出走托盘菜单-退出） */
        ShowWindow(h, SW_HIDE);
        tray_notify("仍在后台运行", "已最小化到托盘，右键托盘图标可快捷操作。");
        return 0;
    }
    return CallWindowProcW(s_prevMainProc, h, m, w, l);
}

void tray_hook_main_window(void)
{
    s_hMainWnd = FindWindowW(NULL, APP_TITLE_W);
    if (!s_hMainWnd) return;
    s_prevMainProc = (WNDPROC)SetWindowLongPtrW(s_hMainWnd, GWLP_WNDPROC,
                                (LONG_PTR)MainWndProc);
}
int tray_init(void)
{
    WNDCLASSW wc;
    static const WCHAR cls[] = L"KptNkTrayMsg";

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = cls;
    if (!RegisterClassW(&wc))
        return -1;

    s_hMsgWnd = CreateWindowExW(0, cls, L"", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, NULL, wc.hInstance, NULL);
    if (!s_hMsgWnd)
        return -1;

    memset(&s_nid, 0, sizeof(s_nid));
    s_nid.cbSize = sizeof(s_nid);
    s_nid.hWnd = s_hMsgWnd;
    s_nid.uID = 1;
    s_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    s_nid.uCallbackMessage = WM_APP_TRAY;
    /* 托盘图标：内嵌 ICO（零外部依赖），失败再试外部文件，最后系统默认 */
    s_nid.hIcon = (HICON)CreateIconFromResourceEx((PBYTE)kIconIco_data,
                                                  kIconIco_len, TRUE, 0x00030000,
                                                  0, 0, LR_DEFAULTCOLOR);
    if (!s_nid.hIcon)
        s_nid.hIcon = (HICON)LoadImageA(NULL, "assets\\icon.ico", IMAGE_ICON, 0, 0,
                       LR_LOADFROMFILE);
    if (!s_nid.hIcon)
        s_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    lstrcpynW(s_nid.szTip, L"" APP_TITLE_W, 128);
    s_added = Shell_NotifyIconW(NIM_ADD, &s_nid);
    return s_added ? 0 : -1;
}

void tray_shutdown(void)
{
    if (s_added)
        Shell_NotifyIconW(NIM_DELETE, &s_nid);
    if (s_hMsgWnd)
        DestroyWindow(s_hMsgWnd);
    s_added = FALSE;
}

int tray_poll(void)
{
    MSG msg;
    int action = 0;

    /* 只处理托盘消息窗口的消息（关键：不能抢主窗口的输入消息，
     * 否则 raylib 的鼠标按键边沿检测会因 prev=curr 快照时序而丢失） */
    while (PeekMessageW(&msg, s_hMsgWnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* 读取存储的命令 */
    if (s_hMsgWnd) {
        LONG_PTR cmd = GetWindowLongPtrW(s_hMsgWnd, GWLP_USERDATA);
        if (cmd) {
            SetWindowLongPtrW(s_hMsgWnd, GWLP_USERDATA, 0);
            switch ((int)cmd) {
            case IDM_TRAY_SHOW:    action = 1; break;
            case IDM_TRAY_EXIT:    action = 2; break;
            case IDM_TRAY_REFRESH: action = 3; break;
            case IDM_TRAY_KILL_NODE: action = 4; break;
            case IDM_TRAY_KILL_PY:  action = 5; break;
            case IDM_TRAY_ORPHAN:   action = 6; break;
            case IDM_TRAY_SETTINGS: action = 7; break;
            case IDM_TRAY_AUTORUN:  action = 8; break;
            case IDM_TRAY_AI_CLEAN: action = 9; break;
            case IDM_TRAY_BASE_SAVE: action = 10; break;
            case IDM_TRAY_BASE_CMP: action = 11; break;
            }
        }
    }
    return action;
}

void MinimizeToTray(void)
{
    HWND h = FindWindowA(NULL, APP_TITLE_A);
    if (h)
        ShowWindow(h, SW_HIDE);
}

void tray_notify(const char *title, const char *text)
{
    if (!s_added)
        return;
    WCHAR wt[128], tx[256];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wt, 128);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, tx, 256);
    lstrcpynW(s_nid.szInfoTitle, wt, 128);
    lstrcpynW(s_nid.szInfo, tx, 256);
    s_nid.uTimeout = 3000;
    s_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &s_nid);
}
int tray_toggle_main_window(void){    HWND h = FindWindowW(NULL, APP_TITLE_W);    if (!h) return 0;    if (IsWindowVisible(h)) {        ShowWindow(h, SW_HIDE);        return 0;    }    ShowWindow(h, SW_SHOW);    SetForegroundWindow(h);    return 1;}