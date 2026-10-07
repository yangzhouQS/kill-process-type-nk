# patch9.py — 内存cap + AI上下文 + 进程详情 + 导出CSV + 终止统计
import io
import re

# ============ 0) sys_bridge: 进程详情查询 ============
P = r"src\sys_bridge.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "SysProcDetail" not in t:
    t = t.replace("unsigned char *SysExtractIconRGBA(",
"""typedef struct {
    unsigned long pid;
    char title[128];      /* 主窗口标题（无窗口为空） */
    unsigned long threads;
    unsigned long handles;
    char startTime[32];   /* 进程启动时间 */
} SysProcDetail;

int SysQueryProcDetail(unsigned long pid, SysProcDetail *out);

unsigned char *SysExtractIconRGBA(""")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("sys_bridge.h ok")

P = r"src\sys_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "SysQueryProcDetail" not in t:
    t = t.replace('#include <shlobj.h>', '#include <shlobj.h>\n#include <tlhelp32.h>')
    fn = r'''
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

int SysQueryProcDetail(unsigned long pid, SysProcDetail *out)
{
    ZeroMemory(out, sizeof(*out));
    out->pid = pid;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h)
        return 1;

    DWORD handles = 0;
    if (GetProcessHandleCount(h, &handles))
        out->handles = handles;

    FILETIME ftC, ftX, ftK, ftU;
    if (GetProcessTimes(h, &ftC, &ftX, &ftK, &ftU)) {
        SYSTEMTIME st;
        FileTimeToSystemTime(&ftC, &st);
        snprintf(out->startTime, sizeof(out->startTime),
                 "%04u-%02u-%02u %02u:%02u:%02u",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    }
    CloseHandle(h);

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
'''
    t = t + "\n" + fn
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("sys_bridge.c ok")

# ============ 1) app_shared: AI 输出 cap + 上下文 ============
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

# AI 输出 cap 4MB
rep("""    int used = (int)strlen(gApp.aiOutput);
    if (used + addLen + 1 > gApp.aiOutputCap) {
        while (used + addLen + 1 > gApp.aiOutputCap)
            gApp.aiOutputCap *= 2;
        gApp.aiOutput = (char *)realloc(gApp.aiOutput, (size_t)gApp.aiOutputCap);
    }
    memcpy(gApp.aiOutput + used, text, (size_t)addLen + 1);""",
"""    int used = (int)strlen(gApp.aiOutput);
    if (used + addLen + 1 > gApp.aiOutputCap) {
        while (used + addLen + 1 > gApp.aiOutputCap)
            gApp.aiOutputCap *= 2;
        gApp.aiOutput = (char *)realloc(gApp.aiOutput, (size_t)gApp.aiOutputCap);
    }
    memcpy(gApp.aiOutput + used, text, (size_t)addLen + 1);
    /* 长跑内存保护：输出超 4MB 截断保留尾部 */
    if (used + addLen + 1 > 4 * 1024 * 1024) {
        memmove(gApp.aiOutput, gApp.aiOutput + used - 1024 * 1024, 1024 * 1024 + addLen + 1);
        used = 1024 * 1024;
        gApp.aiOutput[used] = 0;
        gApp.aiOutput[0] = '[';
        gApp.aiOutput[1] = ' ';
        gApp.aiOutput[2] = ']';
        used = 3;
    }""", "ai-cap")

# AI 对话上下文：发送时携带最近输出作为上下文
rep("""        if (DrawTextButton(A_SEND, (Rectangle){inR.x + inR.width + 8, inR.y, 76, 34}, 1)
            && gApp.aiInputLen && !gApp.aiRunning) {
            AiAppendOut("\\n[我] ");
            AiAppendOut(gApp.aiInput);
            AiStartChat(gApp.aiInput);
            gApp.aiInputLen = 0;
            gApp.aiInput[0] = 0;
        }""",
"""        if (DrawTextButton(A_SEND, (Rectangle){inR.x + inR.width + 8, inR.y, 88, 40}, 1)
            && gApp.aiInputLen && !gApp.aiRunning) {
            AiAppendOut("\\n[我] ");
            AiAppendOut(gApp.aiInput);
            /* 多轮上下文：携带最近对话尾部 */
            char ctx[4096];
            int hlen = (int)strlen(gApp.aiOutput);
            if (hlen > 3000) hlen = 3000;
            ctx[0] = 0;
            if (hlen > 0) {
                int srcOff = (int)strlen(gApp.aiOutput) - hlen;
                /* 跳到 UTF-8 边界 */
                while (srcOff > 0 && (gApp.aiOutput[srcOff] & 0xC0) == 0x80) srcOff--;
                snprintf(ctx, sizeof(ctx), "（最近对话上下文：）\\n%s\\n", gApp.aiOutput + srcOff);
            }
            AiStartChatCtx(ctx, gApp.aiInput);
            gApp.aiInputLen = 0;
            gApp.aiInput[0] = 0;
        }""", "ai-ctx-send")

# AiChatSubmit 同步
rep("""void AiChatSubmit(void)
{
    if (gApp.aiInputLen && !gApp.aiRunning) {
        AiAppendOut("\\n[我] ");
        AiAppendOut(gApp.aiInput);
        AiStartChat(gApp.aiInput);
        gApp.aiInputLen = 0;
        gApp.aiInput[0] = 0;
    }
}""",
"""void AiChatSubmit(void)
{
    if (gApp.aiInputLen && !gApp.aiRunning) {
        AiAppendOut("\\n[我] ");
        AiAppendOut(gApp.aiInput);
        char ctx[4096];
        int hlen = gApp.aiOutput ? (int)strlen(gApp.aiOutput) : 0;
        if (hlen > 3000) hlen = 3000;
        ctx[0] = 0;
        if (hlen > 0) {
            int srcOff = (int)strlen(gApp.aiOutput) - hlen;
            while (srcOff > 0 && (gApp.aiOutput[srcOff] & 0xC0) == 0x80) srcOff--;
            snprintf(ctx, sizeof(ctx), "（最近对话上下文：）\\n%s\\n", gApp.aiOutput + srcOff);
        }
        AiStartChatCtx(ctx, gApp.aiInput);
        gApp.aiInputLen = 0;
        gApp.aiInput[0] = 0;
    }
}""", "ai-ctx-submit")

# 详情/统计弹窗绘制声明实现（modal 3/4）
detail_impl = r'''
/* ================= 进程详情弹窗（modal=3） ================= */

static char sDetailBuf[2048];
static unsigned long sDetailPid = 0;

void OpenProcDetail(unsigned long pid)
{
    BridgeProc *p = NULL;
    for (size_t i = 0; i < gApp.procs.count; i++)
        if (gApp.procs.items[i].pid == pid) { p = &gApp.procs.items[i]; break; }
    if (!p) return;

    SysProcDetail d;
    SysQueryProcDetail(pid, &d);
    sDetailPid = pid;

    snprintf(sDetailBuf, sizeof(sDetailBuf),
             "进程详情\n\n"
             "名称：    %s\n"
             "PID：     %lu\n"
             "父PID：   %lu\n"
             "内存：    %.1f MB\n"
             "类型：    %s\n"
             "AI风险：  %s\n"
             "线程数：  %lu\n"
             "句柄数：  %lu\n"
             "启动时间：%s\n"
             "窗口标题：%s\n\n"
             "可执行路径：\n  %s\n\n"
             "命令行：\n  %s\n"
             "项目：\n  %s",
             p->name, (unsigned long)p->pid, (unsigned long)p->ppid,
             (double)p->memBytes / 1048576.0, ProcTypeName(p->type),
             p->aiRisk ? (p->aiRisk >= 3 ? "高" : p->aiRisk == 2 ? "中" : "低") : "-",
             (unsigned long)d.threads, (unsigned long)d.handles,
             d.startTime[0] ? d.startTime : "-",
             d.title[0] ? d.title : "(无窗口)",
             p->path[0] ? p->path : P_NOREAD,
             p->cmdline[0] ? p->cmdline : "-",
             p->project[0] ? p->project : P_NOPROJ);
    gApp.modal = 3;
}

void DrawProcDetailModal(void)
{
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    float pw = 760, ph = 640;
    DrawModalPanel(pw, ph);
    float px = (W - pw) / 2, py = (H - ph) / 2;

    DrawTxt("进程详情", px + 20, py + 14, FS_TITLE, gPal.onSurface);
    if (DrawTextButton(A_CLOSE, (Rectangle){px + pw - 104, py + 12, 84, 38}, 1)) {
        gApp.modal = 0;
        return;
    }

    Rectangle outR = {px + 16, py + 60, pw - 32, ph - 76};
    DrawRectangleRounded(outR, 0.03f, 6, gPal.surfaceVariant);

    float rows = (float)WrapCount(sDetailBuf, outR.width - 20, FS_TXT) + 2;
    int baseY = BeginList(outR.x, outR.y, outR.width, outR.height, rows, &gApp.scrollAiOut);
    DrawRich(sDetailBuf, outR.x + 10, (float)baseY + 4, outR.width - 20, FS_TXT,
             gPal.onSurface, &gApp.scrollAiOut);
    EndList();
}

/* ================= 终止历史统计（modal=4） ================= */

typedef struct {
    char name[64];
    int total, ok, fail;
} StatRow;

static StatRow sStats[128];
static int sStatCount = 0;

void BuildKillStats(void)
{
    sStatCount = 0;
    for (size_t i = 0; i < gApp.logs.count && sStatCount < 128; i++) {
        BridgeLog *l = &gApp.logs.items[i];
        int found = 0;
        for (int j = 0; j < sStatCount; j++) {
            if (strcmp(sStats[j].name, l->name) == 0) {
                sStats[j].total++;
                if (l->ok) sStats[j].ok++; else sStats[j].fail++;
                found = 1;
                break;
            }
        }
        if (!found) {
            snprintf(sStats[sStatCount].name, 64, "%s", l->name);
            sStats[sStatCount].total = 1;
            sStats[sStatCount].ok = l->ok ? 1 : 0;
            sStats[sStatCount].fail = l->ok ? 0 : 1;
            sStatCount++;
        }
    }
}

static const char *gColsStats[] = {"进程名", "次数", "成功", "失败", "成功率"};
static const float gCwStats[] = {260, 110, 110, 110, 150};

void DrawStatsModal(void)
{
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    float pw = 820, ph = 620;
    DrawModalPanel(pw, ph);
    float px = (W - pw) / 2, py = (H - ph) / 2;

    DrawTxt("终止历史统计", px + 20, py + 14, FS_TITLE, gPal.onSurface);
    if (DrawTextButton(A_CLOSE, (Rectangle){px + pw - 104, py + 12, 84, 38}, 1)) {
        gApp.modal = 0;
        return;
    }

    float x = px + 16, y = py + 60, w = pw - 32, h = ph - 80;
    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.03f, 6, gPal.surfaceVariant);
    DrawTableHeader(x, y, w, gColsStats, gCwStats, 5, NULL, NULL);

    float rowY = y + 44;
    for (int i = 0; i < sStatCount; i++) {
        if (rowY > y + h) break;
        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        char cells[5][220];
        snprintf(cells[0], 220, "%s", sStats[i].name);
        snprintf(cells[1], 220, "%d", sStats[i].total);
        snprintf(cells[2], 220, "%d", sStats[i].ok);
        snprintf(cells[3], 220, "%d", sStats[i].fail);
        snprintf(cells[4], 220, "%d%%",
                 sStats[i].total ? sStats[i].ok * 100 / sStats[i].total : 0);
        const char *cellsP[5] = {cells[0], cells[1], cells[2], cells[3], cells[4]};
        Color colors[5] = {gPal.onSurface, gPal.onSurface, gPal.successC,
                           gPal.errorC, gPal.onSurface};
        DrawRowCells(cellsP, colors, gCwStats, 5, x, rowY, 0);
        rowY += ROW_H;
    }
}
'''
anchor = "/* ================= 主程序 ================= */"
# app_shared.c 无此注释——插到文件尾
if "DrawProcDetailModal" not in t:
    t = t + "\n" + detail_impl

# main.c modal 3/4 分发（后面单独处理 main.c）
with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("app_shared MISS:", miss if miss else "none")

# ============ ai_bridge: AiStartChatCtx + AiParseCleanJson 已有 ============
P = r"src\ai_bridge.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "AiStartChatCtx" not in t:
    t = t.replace("void AiStartChat(const char *msg);",
                  "void AiStartChat(const char *msg);\nvoid AiStartChatCtx(const char *context, const char *msg); /* 多轮上下文 */")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("ai_bridge.h ok")

P = r"src\ai_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "AiStartChatCtx" not in t:
    t = t.replace("""void AiStartChat(const char *msg)
{
    char prompt[16384];
    snprintf(prompt, sizeof(prompt),
             "你是进程管理工具内置助手，用中文简洁回答。问题：%s", msg);
    AiLaunch(prompt);
}""",
"""void AiStartChat(const char *msg)
{
    AiStartChatCtx(NULL, msg);
}

void AiStartChatCtx(const char *context, const char *msg)
{
    static char prompt[20000];
    if (context && context[0])
        snprintf(prompt, sizeof(prompt),
                 "你是进程管理工具内置助手，用中文简洁回答。\\n%s\\n问题：%s",
                 context, msg);
    else
        snprintf(prompt, sizeof(prompt),
                 "你是进程管理工具内置助手，用中文简洁回答。问题：%s", msg);
    AiLaunch(prompt);
}""")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("ai_bridge.c ok")
