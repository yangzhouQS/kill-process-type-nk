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
        "H:\\2026code\\2028-amis\\zcode-demo\\kilo-windows-x64-v7.6.2\\kilo.exe",
        "D:\\kilo\\kilo-windows-x64\\kilo.exe",
    };
    for (int i = 0; i < 2; i++) {
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

    /* 命令行转义：外层引号，内部 " -> \" */
    const char *s = prompt;
    char *w = cmd;
    w += snprintf(w, 64, "\"%s\" run \"", kilo);
    for (; *s && w - cmd < 32000; s++) {
        if (*s == '"')
            *w++ = '\\';
        *w++ = *s;
    }
    *w++ = '"';
    *w = 0;

    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0))
        goto fail;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = wr;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        CloseHandle(rd);
        CloseHandle(wr);
        goto fail;
    }
    CloseHandle(wr);

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
    sResult = _strdup("启动 kilo 失败。");
    sState = 3;
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
    char prompt[16384];
    snprintf(prompt, sizeof(prompt),
             "你是Windows进程管理工具「Node/Python 进程终结者」的内置助手。"
             "当前机器上有多个 node/python 进程。请用中文简洁回答用户问题。\n\n"
             "用户问题：%s", msg);
    AiLaunch(prompt);
}

void AiStartAnalyze(const BridgeProc *p, const char *portsText)
{
    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
             "你是Windows进程管理专家。分析以下进程并评估终止它的风险，"
             "用中文分点简洁回答（300字内）："
             "1)该进程是什么（服务/程序/常见用途）；"
             "2)终止风险评级：低/中/高；"
             "3)评级依据（系统关键性、父进程关系、未保存数据丢失、是否会自动重启、"
             "对监听服务的影响）；4)建议操作。"
             "进程信息：名称=%s；PID=%lu；父PID=%lu；内存=%.1fMB；类型=%s；路径=%s；"
             "监听端口=%s。",
             p->name, (unsigned long)p->pid, (unsigned long)p->ppid,
             (double)p->memBytes / 1048576.0, ProcTypeStr(p->type),
             p->path[0] ? p->path : "(无法读取)",
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
    char line[512];
    int n = snprintf(line, sizeof(line),
                     "PID=%lu 名称=%s 内存=%.0fMB 路径=%s 命令行=%s\n",
                     (unsigned long)p->pid, p->name,
                     (double)p->memBytes / 1048576.0,
                     p->path[0] ? p->path : "(无法读取)",
                     p->cmdline[0] ? p->cmdline : "-");
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
    for (size_t i = 0; i < pl->count && used < (int)sizeof(buf) - 600; i++) {
        if (pl->items[i].type == 0) continue;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[i]);
        count++;
    }
    if (count == 0) {
        used += snprintf(buf + used, sizeof(buf) - used, "(无 Node/Python 进程)\n");
    }
    AiLaunch(buf);
}

void AiStartLogReview(const BridgeLogList *logs)
{
    static char buf[16384];
    int used = snprintf(buf, sizeof(buf),
        "你是Windows进程管理专家。以下是进程终止操作日志（时间|来源|进程名|PID|结果|路径）。"
        "请用中文复盘：1)成功率与失败原因归类；2)可疑或高频被杀对象；3)操作建议（300字内）。\n日志：\n");
    for (size_t i = 0; i < logs->count && used < (int)sizeof(buf) - 400; i++) {
        const BridgeLog *l = &logs->items[i];
        used += snprintf(buf + used, sizeof(buf) - used, "%s|%s|%s|%lu|%s|%s\n",
                         l->timeText, l->source, l->name,
                         (unsigned long)l->pid, l->ok ? "OK" : "FAIL",
                         l->path[0] ? l->path : "-");
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
    for (size_t i = 0; i < pl->count && used < (int)sizeof(buf) - 600; i++) {
        if (pl->items[i].type == 0) continue;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[i]);
        count++;
    }
    if (count == 0) {
        used += snprintf(buf + used, sizeof(buf) - used, "(无 Node/Python 进程)\n");
    }
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