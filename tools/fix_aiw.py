# fix_aiw.py — AiWorker 改 CreateProcessW（UTF-8 命令行 -> UTF-16）
import io
import re

P = r"src\ai_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

# 替换整个 AiWorker 函数（到 fail: 标签前保持，之后保留）
old = re.search(r'static DWORD WINAPI AiWorker\(LPVOID arg\)\n\{.*?\n    \{\n        STARTUPINFOA si;', t, re.S)
assert old, "AiWorker head not found"

new_head = '''static DWORD WINAPI AiWorker(LPVOID arg)
{
    char *prompt = (char *)arg;
    char kilo[MAX_PATH];
    WCHAR kiloW[MAX_PATH * 2];
    WCHAR cmdW[32768];
    char *out = NULL;
    size_t outCap = 0, outLen = 0;

    if (!FindKilo(kilo, sizeof(kilo))) {
        EnterCriticalSection(&sCs);
        sResult = _strdup("未找到 kilo.exe（可设置环境变量 KILO_EXE 指定路径）");
        sState = 3;
        LeaveCriticalSection(&sCs);
        free(prompt);
        return 0;
    }

    /* 净化：换行/回车/制表 -> 空格（命令行参数不能含换行） */
    {
        char clean[16384];
        char cmd[32768];
        size_t ci = 0;
        for (const char *q = prompt; *q && ci < sizeof(clean) - 1; q++)
            clean[ci++] = (*q == '\\n' || *q == '\\r' || *q == '\\t') ? ' ' : *q;
        clean[ci] = 0;
        /* 引号转义后拼 "run \\"...\\"" */
        {
            const char *s = clean;
            char *w = cmd;
            w += snprintf(w, 128, "run \\"");
            for (; *s && w - cmd < 32000; s++) {
                if (*s == '"')
                    *w++ = '\\\\';
                *w++ = *s;
            }
            *w++ = '"';
            *w = 0;
        }
        /* UTF-8 -> UTF-16（中文命令行必须宽字符，否则 GBK 乱码） */
        MultiByteToWideChar(CP_UTF8, 0, cmd, -1, cmdW, 32768);
        MultiByteToWideChar(CP_UTF8, 0, kilo, -1, kiloW, MAX_PATH * 2);
    }

    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE rd = NULL, wr = NULL, inRd = NULL, inWr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0))
        goto fail;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    /* stdin：空管道（立即 EOF，避免 USESTDHANDLES 下 NULL 句柄导致启动失败） */
    if (!CreatePipe(&inRd, &inWr, &sa, 0)) {
        CloseHandle(rd);
        CloseHandle(wr);
        goto fail;
    }
    SetHandleInformation(inRd, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    {
        STARTUPINFOW si;'''

t = t[:old.start()] + new_head + t[old.end():]

# 修正后续：STARTUPINFOA 残余声明与 CreateProcessA
t = t.replace("""    {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = inRd;
        si.hStdOutput = wr;
        si.hStdError = wr;
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(kilo, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                            NULL, NULL, &si, &pi)) {""",
"""    {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = inRd;
        si.hStdOutput = wr;
        si.hStdError = wr;
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessW(kiloW, cmdW, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                            NULL, NULL, &si, &pi)) {""")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("AiWorker rewritten")
