/* sys_bridge.c — Win32 系统操作实现 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <shlobj.h>

#include "sys_bridge.h"

#define SINGLE_MUTEX_A "kpt_nk_single_instance_mutex"
#define MAIN_WINDOW_TITLE "kill-process-type-nk"

int SysRelaunch(const char *path, const char *cmdline)
{
    WCHAR wpath[MAX_PATH * 2], wcmd[2048];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH * 2) <= 0)
        return 1;
    if (MultiByteToWideChar(CP_UTF8, 0, cmdline, -1, wcmd, 2048) <= 0)
        return 1;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(wpath, wcmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return 1;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

int SysShowInExplorer(const char *path)
{
    WCHAR wpath[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH * 2) <= 0)
        return 1;
    WCHAR wparams[MAX_PATH * 2 + 16];
    swprintf(wparams, MAX_PATH * 2 + 16, L"/select,\"%ls\"", wpath);
    HINSTANCE r = ShellExecuteW(NULL, L"open", L"explorer.exe", wparams, NULL, SW_SHOWNORMAL);
    return (INT_PTR)r <= 32;
}

int SysOpenTerminal(const char *dir)
{
    WCHAR wdir[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, MAX_PATH * 2) <= 0)
        return 1;
    HINSTANCE r = ShellExecuteW(NULL, L"open", L"cmd.exe", L"/k", wdir, SW_SHOWNORMAL);
    return (INT_PTR)r <= 32;
}

int SysOpenUrl(const char *url)
{
    WCHAR wurl[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1024) <= 0)
        return 1;
    HINSTANCE r = ShellExecuteW(NULL, L"open", wurl, NULL, NULL, SW_SHOWNORMAL);
    return (INT_PTR)r <= 32;
}

void SysCopyFixCommand(int port, char *buf, int cap)
{
    snprintf(buf, (size_t)cap,
             ":: 释放被 winnat 保留的端口（在管理员终端执行）\r\n"
             "net stop winnat\r\n"
             "net start winnat\r\n"
             ":: 永久保留端口给本服务，防止再次被随机圈走\r\n"
             "netsh int ipv4 add excludedportrange protocol=tcp startport=%d numberofports=1 store=persistent\r\n"
             "netsh interface ipv4 show excludedportrange protocol=tcp\r\n",
             port);
}

int SysElevatedFix(int port)
{
    WCHAR params[1024];
    swprintf(params, 1024,
             L"/c \"title 修复 winnat 保留端口 & "
             L"net stop winnat& net start winnat& "
             L"netsh int ipv4 add excludedportrange protocol=tcp startport=%d numberofports=1 store=persistent& "
             L"netsh interface ipv4 show excludedportrange protocol=tcp& "
             L"echo.& echo 完成。可关闭本窗口后回到工具刷新端口列表验证。& pause\"",
             port);

    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = params;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        DWORD e = GetLastError();
        return (e == ERROR_CANCELLED) ? 1 : 2;
    }
    return 0;
}

int SysSingleInstance(void)
{
    HANDLE m = CreateMutexA(NULL, TRUE, SINGLE_MUTEX_A);
    if (m && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (m) CloseHandle(m);
        SysFlashTrayWindow();
        return 1;
    }
    /* 持有句柄至进程退出（故意不关闭） */
    return 0;
}

void SysFlashTrayWindow(void)
{
    HWND h = FindWindowA(NULL, MAIN_WINDOW_TITLE);
    if (h) {
        ShowWindow(h, SW_SHOW);
        SetForegroundWindow(h);
    }
}

/* ---------- 开机自启动（HKCU Run） ---------- */

static const WCHAR RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const WCHAR RUN_VALUE[] = L"KillProcessTypeNk";

int SysIsAutoRun(void)
{
    HKEY k;
    DWORD type = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return 0;
    LONG r = RegQueryValueExW(k, RUN_VALUE, NULL, &type, NULL, NULL);
    RegCloseKey(k);
    return r == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ);
}

int SysSetAutoRun(int on)
{
    if (!on) {
        HKEY k;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS)
            return 1;
        LONG r = RegDeleteValueW(k, RUN_VALUE);
        RegCloseKey(k);
        return (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND) ? 0 : 1;
    }
    WCHAR exe[MAX_PATH];
    WCHAR cmd[MAX_PATH + 16];
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH))
        return 1;
    swprintf(cmd, MAX_PATH + 16, L"\"%ls\"", exe);
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS)
        return 1;
    LONG r = RegSetValueExW(k, RUN_VALUE, 0, REG_SZ, (const BYTE *)cmd,
                            (DWORD)((lstrlenW(cmd) + 1) * sizeof(WCHAR)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS ? 0 : 1;
}
unsigned char *SysExtractIconRGBA(const char *exePath, int size, int *outW, int *outH){    WCHAR wpath[MAX_PATH * 2];    HICON hIcon = NULL;    *outW = 0; *outH = 0;    if (MultiByteToWideChar(CP_UTF8, 0, exePath, -1, wpath, MAX_PATH * 2) <= 0)        return NULL;    if (ExtractIconExW(wpath, 0, NULL, &hIcon, 1) != 1 || !hIcon)        return NULL;        ICONINFO ii;    if (!GetIconInfo(hIcon, &ii)) { DestroyIcon(hIcon); return NULL; }        BITMAP bm;    if (!GetObjectW(ii.hbmColor, sizeof(bm), &bm)) {        DeleteObject(ii.hbmColor); DeleteObject(ii.hbmMask); DestroyIcon(hIcon);        return NULL;    }    int w = bm.bmWidth, h = bm.bmHeight;        BITMAPINFO bi;    ZeroMemory(&bi, sizeof(bi));    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);    bi.bmiHeader.biWidth = w;    bi.bmiHeader.biHeight = -h; /* top-down */    bi.bmiHeader.biPlanes = 1;    bi.bmiHeader.biBitCount = 32;    bi.bmiHeader.biCompression = BI_RGB;        unsigned char *px = (unsigned char *)malloc((size_t)w * h * 4);    if (!px) {        DeleteObject(ii.hbmColor); DeleteObject(ii.hbmMask); DestroyIcon(hIcon);        return NULL;    }    HDC dc = GetDC(NULL);    int lines = GetDIBits(dc, ii.hbmColor, 0, h, px, &bi, DIB_RGB_COLORS);    ReleaseDC(NULL, dc);    DeleteObject(ii.hbmColor);    DeleteObject(ii.hbmMask);    DestroyIcon(hIcon);    if (lines != h) { free(px); return NULL; }        /* BGRA -> RGBA */    for (int i = 0; i < w * h; i++) {        unsigned char b = px[i * 4 + 0];        px[i * 4 + 0] = px[i * 4 + 2];        px[i * 4 + 2] = b;    }    *outW = w; *outH = h;    return px;}