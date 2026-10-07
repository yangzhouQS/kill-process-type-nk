/* findwin_test.c — 诊断：FindWindow 两种标题匹配 + EnumWindows 原始标题打印 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    DWORD wantPid = argc > 1 ? (DWORD)strtoul(argv[1], NULL, 10) : 0;

    /* clickx 模式：真实鼠标点击标题栏 X 按钮 */
    if (argc > 1 && strcmp(argv[1], "clickx") == 0) {
        FILE *log = fopen("build/clickx_out.txt", "w");
        if (!log) return 1;
        HWND w = FindWindowW(NULL, L"kill-process-type-nk v7.0.0");
        if (!w) { fprintf(log, "window not found\n"); fclose(log); return 1; }
        ShowWindow(w, SW_RESTORE);
        SetForegroundWindow(w);
        Sleep(800);
        RECT r;
        GetWindowRect(w, &r);
        int cx = r.right - 45, cy = r.top + 25;
        SetCursorPos(cx, cy);
        Sleep(200);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(60);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        fprintf(log, "clicked X at screen (%d,%d)\n", cx, cy);
        Sleep(2000);
        HWND again = FindWindowW(NULL, L"kill-process-type-nk v7.0.0");
        fprintf(log, "after click: hwnd=%p visible=%d\n",
                (void *)again, again ? IsWindowVisible(again) : -1);
        fclose(log);
        return 0;
    }

    HWND a = FindWindowW(NULL, L"kill-process-type-nk v7.0.0");
    printf("FindWindowW(full title):      %p\n", (void *)a);
    HWND b = FindWindowW(NULL, L"kill-process-type-nk");
    printf("FindWindowW(no version):      %p\n", (void *)b);
    HWND c = FindWindowA(NULL, "kill-process-type-nk v7.0.0");
    printf("FindWindowA(full title):      %p\n", (void *)c);

    printf("--- EnumWindows pid=%lu ---\n", (unsigned long)wantPid);
    /* 手动枚举 */
    HWND h = GetTopWindow(NULL);
    while (h) {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid == wantPid) {
            WCHAR t[256];
            int n = GetWindowTextW(h, t, 256);
            char utf8[512];
            WideCharToMultiByte(CP_UTF8, 0, t, -1, utf8, sizeof(utf8), NULL, NULL);
            printf("  hwnd=%p visible=%d len=%d title=[%s]\n",
                   (void *)h, IsWindowVisible(h) ? 1 : 0, n, utf8);
        }
        h = GetNextWindow(h, GW_HWNDNEXT);
    }
    return 0;
}
