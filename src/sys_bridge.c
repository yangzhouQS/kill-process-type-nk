/* sys_bridge.c — Win32 系统操作实现 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include "sys_bridge.h"
#include "version.h"

#define SINGLE_MUTEX_A "kpt_nk_single_instance_mutex"
#define MAIN_WINDOW_TITLE APP_TITLE_A

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
    WCHAR wdir[MAX_PATH * 2], params[MAX_PATH * 2 + 16];
    if (MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, MAX_PATH * 2) <= 0)
        return 1;
    swprintf(params, MAX_PATH * 2 + 16, L"/k cd /d \"%ls\"", wdir);
    HINSTANCE r = ShellExecuteW(NULL, L"open", L"cmd.exe", params, NULL, SW_SHOWNORMAL);
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
    HWND h = FindWindowA(NULL, APP_TITLE_A);
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
unsigned char *SysExtractIconRGBA(const char *exePath, int size, int *outW, int *outH){    WCHAR wpath[MAX_PATH * 2];    HICON hIcon = NULL;    *outW = 0; *outH = 0;    if (MultiByteToWideChar(CP_UTF8, 0, exePath, -1, wpath, MAX_PATH * 2) <= 0)        return NULL;    if (ExtractIconExW(wpath, 0, NULL, &hIcon, 1) != 1 || !hIcon)        return NULL;        ICONINFO ii;    if (!GetIconInfo(hIcon, &ii)) { DestroyIcon(hIcon); return NULL; }        BITMAP bm;    if (!GetObjectW(ii.hbmColor, sizeof(bm), &bm)) {        DeleteObject(ii.hbmColor); DeleteObject(ii.hbmMask); DestroyIcon(hIcon);        return NULL;    }    int w = bm.bmWidth, h = bm.bmHeight;        BITMAPINFO bi;    ZeroMemory(&bi, sizeof(bi));    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);    bi.bmiHeader.biWidth = w;    bi.bmiHeader.biHeight = -h; /* top-down */    bi.bmiHeader.biPlanes = 1;    bi.bmiHeader.biBitCount = 32;    bi.bmiHeader.biCompression = BI_RGB;        unsigned char *px = (unsigned char *)malloc((size_t)w * h * 4);    if (!px) {        DeleteObject(ii.hbmColor); DeleteObject(ii.hbmMask); DestroyIcon(hIcon);        return NULL;    }    HDC dc = GetDC(NULL);    int lines = GetDIBits(dc, ii.hbmColor, 0, h, px, &bi, DIB_RGB_COLORS);    ReleaseDC(NULL, dc);    DeleteObject(ii.hbmColor);    DeleteObject(ii.hbmMask);    DestroyIcon(hIcon);    if (lines != h) { free(px); return NULL; }        /* BGRA -> RGBA */    for (int i = 0; i < w * h; i++) {        unsigned char b = px[i * 4 + 0];        px[i * 4 + 0] = px[i * 4 + 2];        px[i * 4 + 2] = b;    }    *outW = w; *outH = h;    return px;}void SysClampWindowRect(int *x, int *y, int *w, int *h){    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);    if (*w > vw) *w = vw;    if (*h > vh) *h = vh;    if (*x < vx) *x = vx;    if (*x + *w > vx + vw) *x = vx + vw - *w;    if (*y < vy) *y = vy;    if (*y + *h > vy + vh) *y = vy + vh - *h;}int SysRestartElevated(void){    WCHAR exe[MAX_PATH];    if (!GetModuleFileNameW(NULL, exe, MAX_PATH))        return 2;    SHELLEXECUTEINFOW sei;    ZeroMemory(&sei, sizeof(sei));    sei.cbSize = sizeof(sei);    sei.fMask = SEE_MASK_NOASYNC;    sei.lpVerb = L"runas";    sei.lpFile = exe;    sei.nShow = SW_SHOWNORMAL;    if (!ShellExecuteExW(&sei)) {        DWORD e = GetLastError();        return (e == ERROR_CANCELLED) ? 1 : 2;    }    return 0;}

typedef struct {
    DWORD wantPid;
    WCHAR title[128];
} FindTitleCtx;

static BOOL CALLBACK FindTitleProc(HWND h, LPARAM lp)
{
    FindTitleCtx *ctx = (FindTitleCtx *)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != ctx->wantPid || !IsWindowVisible(h))
        return TRUE;
    WCHAR buf[128];
    if (GetWindowTextW(h, buf, 128) > 0) {
        lstrcpynW(ctx->title, buf, 128);
        return FALSE; /* 找到即停 */
    }
    return TRUE;
}

int SysQueryProcDetail(unsigned long pid, unsigned long ppid, SysProcDetail *out)
{
    ZeroMemory(out, sizeof(*out));
    out->pid = pid;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h)
        return 1;

    DWORD handles = 0;
    if (GetProcessHandleCount(h, &handles))
        out->handles = handles;

    /* 特权数 */
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

    FILETIME ftC, ftX, ftK, ftU;
    if (GetProcessTimes(h, &ftC, &ftX, &ftK, &ftU)) {
        SYSTEMTIME st;
        FileTimeToSystemTime(&ftC, &st);
        snprintf(out->startTime, sizeof(out->startTime),
                 "%04u-%02u-%02u %02u:%02u:%02u",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    }
    CloseHandle(h);

    /* 父进程名 */
    {
        HANDLE ph = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)ppid);
        if (ph) {
            WCHAR pname[64];
            DWORD sz = 64;
            if (QueryFullProcessImageNameW(ph, 0, pname, &sz)) {
                WCHAR *base = wcsrchr(pname, L'\\');
                base = base ? base + 1 : pname;
                WideCharToMultiByte(CP_UTF8, 0, base, -1, out->parentName,
                                    sizeof(out->parentName), NULL, NULL);
            }
            CloseHandle(ph);
        }
    }

    /* 线程数 */
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        DWORD cnt = 0;
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == (DWORD)pid)
                    cnt++;
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
        out->threads = cnt;
    }

    /* 主窗口标题 */
    FindTitleCtx ctx;
    ctx.wantPid = (DWORD)pid;
    ctx.title[0] = 0;
    EnumWindows(FindTitleProc, (LPARAM)&ctx);
    WideCharToMultiByte(CP_UTF8, 0, ctx.title, -1, out->title,
                        sizeof(out->title), NULL, NULL);
    return 0;
}


void SysExeDir(char *out, int cap)
{
    WCHAR exe[MAX_PATH];
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) {
        if (cap > 0) out[0] = 0;
        return;
    }
    WCHAR *slash = wcsrchr(exe, L'\\');
    if (slash) *slash = 0;
    WideCharToMultiByte(CP_UTF8, 0, exe, -1, out, cap, NULL, NULL);
}


int SysDownloadsDir(char *out, int cap)
{
    PWSTR w = NULL;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &w)))
        return 1;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL);
    CoTaskMemFree(w);
    if (n <= 0 || n > cap) return 1;
    /* 去掉结尾反斜杠（若有） */
    if (n >= 2 && out[n - 2] == '\\')
        out[n - 2] = 0;
    return 0;
}

void SysTimestamp(char *out, int cap)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(out, cap, "%04d-%02d-%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}
