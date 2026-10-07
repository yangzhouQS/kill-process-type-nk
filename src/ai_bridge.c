/* ai_bridge.c — kilo 无头 CLI 异步调用（CreateThread + 管道 + 超时） */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "ai_bridge.h"

#define AI_TIMEOUT_MS (5 * 60 * 1000)

static volatile int sState = 0; /* 0 空闲 1 运行 2 完成 3 失败 */
static char *sResult = NULL;
static HANDLE sThread = NULL;
static CRITICAL_SECTION sCs;

static BOOL sCsInit = FALSE;

/* 定位 kilo.exe：环境变量 KILO_EXE -> 两个内置路径 */
static BOOL FindKilo(char *out, int cap)
{
    char buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("KILO_EXE", buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf) && GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) {
        snprintf(out, cap, "%s", buf);
        return TRUE;
    }
    static const char *cands[] = {
        "D:\\kilo\\kilo-windows-x64\\kilo.exe",
        "D:\\kilo\\kilo-windows-x64-v7.7.7\\kilo.exe",
        "H:\\2026code\\2028-amis\\zcode-demo\\kilo-windows-x64-v7.6.2\\kilo.exe",
    };
    for (int i = 0; i < 3; i++) {
        if (GetFileAttributesA(cands[i]) != INVALID_FILE_ATTRIBUTES) {
            snprintf(out, cap, "%s", cands[i]);
            return TRUE;
        }
    }
    return FALSE;
}

static const char *ProcTypeStr(int type)
{
    return type == 1 ? "Node.js" : type == 2 ? "Python" : "-";
}

int AiAvailable(void)
{
    char path[MAX_PATH];
    return FindKilo(path, sizeof(path));
}

/* 工作线程：执行 kilo run "<prompt>"，收集 stdout */
static DWORD WINAPI AiWorker(LPVOID arg)
{
    char *prompt = (char *)arg;
    char kilo[MAX_PATH];
    WCHAR kiloW[MAX_PATH * 2];
    WCHAR cmdW[32768];
    char cmd[32768];
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
        size_t ci = 0;
        for (const char *q = prompt; *q && ci < sizeof(clean) - 1; q++)
            clean[ci++] = (*q == '\n' || *q == '\r' || *q == '\t') ? ' ' : *q;
        clean[ci] = 0;
        /* 引号转义后拼 '"kilo路径" run "prompt"'（与 shell 传参格式一致） */
        {
            const char *s = clean;
            char *w = cmd;
            w += snprintf(w, MAX_PATH + 16, "\"%s\" run \"", kilo);
            for (; *s && w - cmd < 32000; s++) {
                if (*s == '"')
                    *w++ = '\\';
                *w++ = *s;
            }
            *w++ = '"';
            *w = 0;
        }
        /* UTF-8 -> UTF-16：中文命令行必须宽字符（ANSI 版会按 GBK 解码成乱码） */
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

    if (!CreateProcessW(NULL, cmdW, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        CloseHandle(rd);
        CloseHandle(wr);
        CloseHandle(inRd);
        CloseHandle(inWr);
        goto fail;
    }
    CloseHandle(wr);
    CloseHandle(inRd);
    /* 注意：inWr 保持打开——kilo 无头仍会读 stdin，EOF 会导致其提前退出；
     * 进程结束后统一关闭 */

    {
        DWORD start = GetTickCount();
        char chunk[4096];
        DWORD n = 0;
        BOOL timeout = FALSE;
        for (;;) {
            if (!PeekNamedPipe(rd, NULL, 0, NULL, &n, NULL)) {
                if (GetTickCount() - start > AI_TIMEOUT_MS) timeout = TRUE;
                if (WaitForSingleObject(pi.hProcess, 200) == WAIT_OBJECT_0) {
                    /* 进程退出，排空管道 */
                    while (ReadFile(rd, chunk, sizeof(chunk), &n, NULL) && n)
                        ;
                    break;
                }
                if (timeout) {
                    TerminateProcess(pi.hProcess, 1);
                    break;
                }
                continue;
            }
            if (n == 0) {
                if (WaitForSingleObject(pi.hProcess, 100) == WAIT_OBJECT_0) {
                    while (ReadFile(rd, chunk, sizeof(chunk), &n, NULL) && n)
                        ;
                    break;
                }
                if (GetTickCount() - start > AI_TIMEOUT_MS) {
                    timeout = TRUE;
                    TerminateProcess(pi.hProcess, 1);
                    break;
                }
                continue;
            }
            if (ReadFile(rd, chunk, sizeof(chunk), &n, NULL) && n) {
                if (outLen + n + 1 > outCap) {
                    outCap = (outLen + n + 1) * 2;
                    out = (char *)realloc(out, outCap);
                }
                memcpy(out + outLen, chunk, n);
                outLen += n;
                out[outLen] = 0;
            }
        }
        CloseHandle(rd);
        CloseHandle(inWr);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        EnterCriticalSection(&sCs);
        if (timeout) {
            free(out);
            sResult = _strdup("分析超时（5 分钟）已终止。");
            sState = 3;
        } else if (out && outLen) {
            sResult = out;
            sState = 2;
        } else {
            free(out);
            sResult = _strdup("kilo 无输出（退出码未知）。");
            sState = 3;
        }
        LeaveCriticalSection(&sCs);
    }
    free(prompt);
    return 0;

fail:
    EnterCriticalSection(&sCs);
    {
        char msg[160];
        snprintf(msg, sizeof(msg), "启动 kilo 失败 (err=%lu)。", (unsigned long)GetLastError());
        sResult = _strdup(msg);
        sState = 3;
    }
    LeaveCriticalSection(&sCs);
    free(prompt);
    return 0;
}

static BOOL AiLaunch(const char *prompt)
{
    if (sState == 1)
        return FALSE;
    if (!sCsInit) {
        InitializeCriticalSection(&sCs);
        sCsInit = TRUE;
    }
    EnterCriticalSection(&sCs);
    sState = 1;
    if (sResult) { free(sResult); sResult = NULL; }
    LeaveCriticalSection(&sCs);

    char *copy = _strdup(prompt);
    sThread = CreateThread(NULL, 0, AiWorker, copy, 0, NULL);
    if (!sThread) {
        free(copy);
        EnterCriticalSection(&sCs);
        sState = 3;
        sResult = _strdup("创建 AI 线程失败。");
        LeaveCriticalSection(&sCs);
        return FALSE;
    }
    CloseHandle(sThread);
    sThread = NULL;
    return TRUE;
}

void AiStartChat(const char *msg)
{
    AiStartChatCtx(NULL, msg);
}

void AiStartChatCtx(const char *context, const char *msg)
{
    static char prompt[20000];
    if (context && context[0])
        snprintf(prompt, sizeof(prompt),
                 "你是进程管理工具内置助手，用中文简洁回答。\n%s\n问题：%s",
                 context, msg);
    else
        snprintf(prompt, sizeof(prompt),
                 "你是进程管理工具内置助手，用中文简洁回答。问题：%s", msg);
    AiLaunch(prompt);
}

void AiStartAnalyze(const BridgeProc *p, const char *portsText)
{
    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
             "你是Windows进程管理专家。分析以下进程终止风险，"
             "中文分点简洁回答（300字内）：1)是什么 2)风险：低/中/高 "
             "3)依据 4)建议。进程：%s PID=%lu 内存=%.0fMB 类型=%s 端口=%s",
             p->name, (unsigned long)p->pid,
             (double)p->memBytes / 1048576.0, ProcTypeStr(p->type),
             portsText && portsText[0] ? portsText : "无");
    AiLaunch(prompt);
}

int AiPoll(void)
{
    if (!sCsInit)
        return 0;
    EnterCriticalSection(&sCs);
    int st = sState;
    LeaveCriticalSection(&sCs);
    return st;
}

const char *AiGetResult(void)
{
    return sResult ? sResult : "";
}

void AiConsumeResult(void)
{
    EnterCriticalSection(&sCs);
    if (sResult) { free(sResult); sResult = NULL; }
    sState = 0;
    LeaveCriticalSection(&sCs);
}

void AiShutdown(void)
{
    AiConsumeResult();
    if (sCsInit)
        DeleteCriticalSection(&sCs);
    sCsInit = FALSE;
}

/* ---------- 扩展：批量/诊断/复盘/策略 ---------- */

static BOOL AppendProcLine(char *buf, int cap, int *used, const BridgeProc *p)
{
    /* 精简格式：kilo run 对长命令行参数有内部 bug（>2000 字符易触发
     * 内部错误/路径误判），故不携带路径与命令行，且总长受限 */
    char line[160];
    int n = snprintf(line, sizeof(line), "%lu|%s|%.0fMB\n",
                     (unsigned long)p->pid, p->name,
                     (double)p->memBytes / 1048576.0);
    if (*used + n >= cap) return FALSE;
    strcat(buf + *used, line);
    *used += n;
    return TRUE;
}

void AiStartRiskScan(const BridgeProcList *pl)
{
    static char buf[32768];
    int used = snprintf(buf, sizeof(buf),
        "你是Windows风险分析专家。对下列每个进程给出终止风险分级。"
        "必须只输出一个JSON数组，不要任何其他文字，格式：\n"
        "[{\"pid\":123,\"risk\":2}]\n"
        "risk: 1=低 2=中 3=高。进程列表：\n");
    int count = 0;
    int total = 0;
    for (size_t i = 0; i < pl->count; i++)
        if (pl->items[i].type != 0) total++;
    for (size_t i = 0; i < pl->count && used < 700 && count < 18; i++) {
        if (pl->items[i].type == 0) continue;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[i]);
        count++;
    }
    if (total > count)
        used += snprintf(buf + used, sizeof(buf) - used, "（其余 %d 个略）\n", total - count);
    if (count == 0)
        used += snprintf(buf + used, sizeof(buf) - used, "(无 Node/Python 进程)\n");
    AiLaunch(buf);
}

void AiStartLogReview(const BridgeLogList *logs)
{
    static char buf[16384];
    int used = snprintf(buf, sizeof(buf),
        "你是Windows进程管理专家。以下是进程终止操作日志（时间|来源|进程名|PID|结果|路径）。"
        "请用中文复盘：1)成功率与失败原因归类；2)可疑或高频被杀对象；3)操作建议（300字内）。\n日志：\n");
    {
        size_t shown = 0;
        for (size_t i = 0; i < logs->count; i++) {
            const BridgeLog *l = &logs->items[i];
            if (used >= 700 || shown >= 18) {
                used += snprintf(buf + used, sizeof(buf) - used,
                                 "（其余 %d 条略）\n", (int)(logs->count - shown));
                break;
            }
            used += snprintf(buf + used, sizeof(buf) - used, "%s|%s|PID%lu|%s\n",
                             l->timeText, l->name, (unsigned long)l->pid,
                             l->ok ? "OK" : "FAIL");
            shown++;
        }
    }
    AiLaunch(buf);
}

void AiStartDiag(const BridgeProcList *pl)
{
    static char buf[32768];
    int used = snprintf(buf, sizeof(buf),
        "你是Windows系统诊断专家。以下是当前内存占用前30的进程（PID/名称/内存/路径）。"
        "请给出：1)系统健康度评估；2)资源占用异常项；3)可优化建议（400字内）。\n进程：\n");
    /* 按内存取前 30：简单选择循环 */
    size_t taken = 0;
    int usedFlag[4096];
    memset(usedFlag, 0, sizeof(usedFlag));
    while (taken < 30 && used < (int)sizeof(buf) - 600) {
        size_t best = pl->count;
        unsigned long long bestMem = 0;
        for (size_t i = 0; i < pl->count && i < 4096; i++) {
            if (usedFlag[i]) continue;
            if (pl->items[i].memBytes > bestMem) {
                bestMem = pl->items[i].memBytes;
                best = i;
            }
        }
        if (best == pl->count) break;
        usedFlag[best] = 1;
        if (used < 700)
            AppendProcLine(buf, sizeof(buf), &used, &pl->items[best]);
        taken++;
    }
    AiLaunch(buf);
}

void AiStartCleanStrategy(const BridgeProcList *pl)
{
    static char buf[32768];
    int used = snprintf(buf, sizeof(buf),
        "你是Windows进程清理策略专家。当前Node/Python相关进程如下（PID/名称/内存/路径/命令行）。"
        "请先输出JSON数组[{\"pid\":123,\"reason\":\"...\"}]列出可安全终止项，"
        "再用中文说明需保留项及原因（300字内）。\n进程：\n");
    int count = 0;
    for (size_t i = 0; i < pl->count && used < 700 && count < 18; i++) {
        if (pl->items[i].type == 0) continue;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[i]);
        count++;
    }
    if (count == 0)
        used += snprintf(buf + used, sizeof(buf) - used, "(无 Node/Python 进程)\n");
    AiLaunch(buf);
}

int AiParseRiskJson(const char *text, unsigned int *pids, int *risks, int max)
{
    const char *p = text;
    int n = 0;
    while (n < max && (p = strstr(p, "\"pid\"")) != NULL) {
        const char *q = strchr(p, ':');
        if (!q) break;
        unsigned long pid = strtoul(q + 1, NULL, 10);
        int risk = 1;
        const char *r = strstr(q, "\"risk\"");
        if (r && r < p + 200) {
            const char *r2 = strchr(r, ':');
            if (r2) risk = atoi(r2 + 1);
            p = r + 5;
        } else {
            p = q + 1;
        }
        if (pid > 0) {
            pids[n] = (unsigned int)pid;
            risks[n] = risk < 1 ? 1 : risk > 3 ? 3 : risk;
            n++;
        }
    }
    return n;
}
int AiParseCleanJson(const char *text, unsigned int *pids, int max)
{
    const char *p = text;
    int n = 0;
    while (n < max && (p = strstr(p, "\"pid\"")) != NULL) {
        const char *q = strchr(p, ':');
        if (!q) break;
        unsigned long pid = strtoul(q + 1, NULL, 10);
        p = q + 1;
        if (pid > 0)
            pids[n++] = (unsigned int)pid;
    }
    return n;
}
