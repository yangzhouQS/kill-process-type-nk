/* tray_bridge.c — Win32 托盘实现（隔离 Win32 头文件，避免与 raylib 冲突） */
#define NOGDI
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#include "tray_bridge.h"

#define WM_APP_TRAY (WM_APP + 1)
#define IDM_TRAY_SHOW    2001
#define IDM_TRAY_EXIT    2002
#define IDM_TRAY_REFRESH 2003
#define IDM_TRAY_KILL_NODE 2004
#define IDM_TRAY_KILL_PY   2005
#define IDM_TRAY_ORPHAN  2006

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
                AppendMenuW(m, MF_SEPARATOR, 0, NULL);
                AppendMenuW(m, MF_STRING, IDM_TRAY_SHOW, L"显示主界面");
                AppendMenuW(m, MF_STRING, IDM_TRAY_EXIT, L"退出");
                SetForegroundWindow(hwnd);
                TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                PostMessage(hwnd, WM_NULL, 0, 0);
                DestroyMenu(m);
            }
        } else if (lp == WM_LBUTTONUP) {
            PostMessage(hwnd, WM_COMMAND, IDM_TRAY_SHOW, 0);
        }
    } else if (msg == WM_COMMAND) {
        /* 存储待处理命令（tray_poll 消费） */
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)LOWORD(wp));
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
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
    s_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    lstrcpynW(s_nid.szTip, L"kill-process-type-nk", 128);
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

    /* 处理所有待处理消息 */
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
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
            }
        }
    }
    return action;
}
