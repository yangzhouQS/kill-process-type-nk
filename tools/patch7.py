# patch7.py — 异步刷新 + Markdown富文本 + CLI /ai
import io

# ============ data_bridge: 异步扫描 ============
P = r"src\data_bridge.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "bridge_scan_async" not in t:
    t = t.replace("/* 进程扫描 */",
"""/* 进程扫描（同步） */
void bridge_scan_processes(BridgeProcList *out);
void bridge_free_processes(BridgeProcList *l);

/* 进程扫描（异步：后台线程，主循环轮询交付） */
int bridge_scan_async_start(void);  /* 1=已启动（忙碌时返回 0） */
int bridge_scan_async_poll(BridgeProcList *procs, BridgePortList *ports); /* 1=完成并交付所有权 */""")
    # 去掉旧的重复声明（上面插入保留原两行，再删原声明区中旧行）
    t = t.replace("""/* 进程扫描（同步） */
void bridge_scan_processes(BridgeProcList *out);
void bridge_free_processes(BridgeProcList *l);

/* 进程扫描（异步：后台线程，主循环轮询交付） */
int bridge_scan_async_start(void);  /* 1=已启动（忙碌时返回 0） */
int bridge_scan_async_poll(BridgeProcList *procs, BridgePortList *ports); /* 1=完成并交付所有权 */
void bridge_scan_processes(BridgeProcList *out);
void bridge_free_processes(BridgeProcList *l);""",
"""/* 进程扫描（同步） */
void bridge_scan_processes(BridgeProcList *out);
void bridge_free_processes(BridgeProcList *l);

/* 进程扫描（异步：后台线程，主循环轮询交付） */
int bridge_scan_async_start(void);  /* 1=已启动（忙碌时返回 0） */
int bridge_scan_async_poll(BridgeProcList *procs, BridgePortList *ports); /* 1=完成并交付所有权 */""")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("bridge.h ok")

P = r"src\data_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "bridge_scan_async_start" not in t:
    impl = '''
/* ---------- 异步扫描（后台线程） ---------- */

#ifdef _WIN32
#include <windows.h>
typedef HANDLE bthread_t;
#else
#include <pthread.h>
typedef pthread_t bthread_t;
#endif

static volatile int sAsyncState = 0; /* 0 空闲 1 运行 2 完成 */
static BridgeProcList sAsyncProcs;
static BridgePortList sAsyncPorts;

#ifdef _WIN32
static DWORD WINAPI AsyncScanWorker(LPVOID arg)
#else
static void *AsyncScanWorker(void *arg)
#endif
{
    (void)arg;
    memset(&sAsyncProcs, 0, sizeof(sAsyncProcs));
    memset(&sAsyncPorts, 0, sizeof(sAsyncPorts));
    bridge_scan_processes(&sAsyncProcs);
    bridge_scan_ports(&sAsyncPorts);
    sAsyncState = 2;
    return 0;
}

int bridge_scan_async_start(void)
{
    if (sAsyncState == 1)
        return 0;
    if (sAsyncState == 2) {
        bridge_free_processes(&sAsyncProcs);
        bridge_free_ports(&sAsyncPorts);
        sAsyncState = 0;
    }
    sAsyncState = 1;
#ifdef _WIN32
    HANDLE th = CreateThread(NULL, 0, AsyncScanWorker, NULL, 0, NULL);
    if (!th) { sAsyncState = 0; return 0; }
    CloseHandle(th);
#else
    pthread_t th;
    if (pthread_create(&th, NULL, AsyncScanWorker, NULL) != 0) { sAsyncState = 0; return 0; }
    pthread_detach(th);
#endif
    return 1;
}

int bridge_scan_async_poll(BridgeProcList *procs, BridgePortList *ports)
{
    if (sAsyncState != 2)
        return 0;
    *procs = sAsyncProcs;
    *ports = sAsyncPorts;
    memset(&sAsyncProcs, 0, sizeof(sAsyncProcs));
    memset(&sAsyncPorts, 0, sizeof(sAsyncPorts));
    sAsyncState = 0;
    return 1;
}
'''
    t = t + "\n" + impl
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("bridge.c ok")

# ============ app_shared.c: Markdown 富文本 ============
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

if "DrawRich" not in t:
    rich = '''
/* ================= Markdown 富文本（AI 输出） =================
 * 行级样式：# ## 标题放大 / - * 列表圆点 / > 引用缩进
 * 行内样式：**加粗**（主色） `代码`（高亮+背景）
 */
static void DrawRichLine(const char *seg, size_t segLen, float x, float y,
                         float maxW, float size, Color base)
{
    char token[512];
    float cx = x;
    size_t i = 0;

    while (i < segLen && cx < x + maxW) {
        if (seg[i] == '*' && i + 1 < segLen && seg[i + 1] == '*') {
            /* **bold** */
            size_t j = i + 2, start = j;
            while (j < segLen && !(seg[j] == '*' && j + 1 < segLen && seg[j + 1] == '*'))
                j++;
            size_t n = j - start;
            if (n > 0 && j + 1 < segLen) {
                if (n > 511) n = 511;
                memcpy(token, seg + start, n);
                token[n] = 0;
                Vector2 ts = MeasureTxt(token, size);
                if (cx + ts.x > x + maxW) break;
                DrawTxt(token, cx, y, size, gPal.primary);
                cx += ts.x;
                i = j + 2;
                continue;
            }
        }
        if (seg[i] == '`') {
            size_t j = i + 1, start = j;
            while (j < segLen && seg[j] != '`')
                j++;
            if (j > start) {
                size_t n = j - start;
                if (n > 511) n = 511;
                memcpy(token, seg + start, n);
                token[n] = 0;
                Vector2 ts = MeasureTxt(token, size);
                if (cx + ts.x > x + maxW) break;
                DrawRectangleRec((Rectangle){cx - 2, y - 1, ts.x + 4, size + 4},
                                 gPal.surfaceVariant);
                DrawTxt(token, cx, y, size, gPal.warnC);
                cx += ts.x;
                i = j + 1;
                continue;
            }
        }
        /* 普通文本：到下一个样式符 */
        size_t j = i;
        while (j < segLen && seg[j] != '*' && seg[j] != '`')
            j++;
        size_t n = j - i;
        if (n > 511) n = 511;
        memcpy(token, seg + i, n);
        token[n] = 0;
        Vector2 ts = MeasureTxt(token, size);
        if (cx + ts.x > x + maxW) {
            /* 超宽截断本行 */
            float avail = x + maxW - cx;
            const char *cl = Clip(token, avail);
            DrawTxt(cl, cx, y, size, base);
            break;
        }
        DrawTxt(token, cx, y, size, base);
        cx += ts.x;
        i = j;
    }
}

void DrawRich(const char *text, float x, float y, float maxW, float size,
              Color base, float *scroll)
{
    (void)scroll;
    float cy = y;
    const char *p = text;
    char line[2048];
    while (*p) {
        const char *nl = strchr(p, '\\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > 2047) len = 2047;
        memcpy(line, p, len);
        line[len] = 0;

        if (line[0] == '#' && line[1] == '#' && line[2] == ' ') {
            DrawRichLine(line + 3, strlen(line + 3), x, cy, maxW, size + 3, gPal.primary);
            cy += size + 10;
        } else if (line[0] == '#' && line[1] == ' ') {
            DrawRichLine(line + 2, strlen(line + 2), x, cy, maxW, size + 6, gPal.primary);
            cy += size + 14;
        } else if ((line[0] == '-' || line[0] == '*') && line[1] == ' ') {
            DrawCircle(x + 6, cy + size / 2, 3, gPal.primary);
            DrawRichLine(line + 2, strlen(line + 2), x + 18, cy, maxW - 18, size, base);
            cy += size + 5;
        } else if (line[0] == '>') {
            DrawRectangleRec((Rectangle){x, cy, 3, size + 2}, gPal.primary);
            DrawRichLine(line + (line[1] == ' ' ? 2 : 1),
                         strlen(line + (line[1] == ' ' ? 2 : 1)),
                         x + 10, cy, maxW - 10, gPal.onSurfaceVariant);
            cy += size + 5;
        } else {
            /* 普通段：自动换行（按富文本 token 简化为单行超宽截断+折行近似） */
            DrawRichLine(line, strlen(line), x, cy, maxW, size, base);
            cy += size + 5;
        }
        p = nl ? nl + 1 : p + len;
        if (*p == 0) break;
    }
}
'''
    anchor = "/* ================= AI 面板 ================= */"
    t = t.replace(anchor, rich + "\n" + anchor, 1)
    # AI 面板输出区换 DrawRich
    rep("""    if (gApp.aiOutput && gApp.aiOutput[0])
        DrawWrapped(gApp.aiOutput, outR.x + 8, (float)baseY + 4,
                    outR.width - 16, FS_TXT, gPal.onSurface, &gApp.scrollAiOut);""",
        """    if (gApp.aiOutput && gApp.aiOutput[0])
        DrawRich(gApp.aiOutput, outR.x + 8, (float)baseY + 4,
                 outR.width - 16, FS_TXT, gPal.onSurface, &gApp.scrollAiOut);""", "ai-rich")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("app_shared MISS:", miss if miss else "none")

# ============ main.c: 异步刷新接线 + CLI /ai ============
P = r"src\main.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss2 = []

def rep2(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss2.append(tag)

rep2("""static void RefreshData(void)
{
    bridge_free_processes(&gApp.procs);
    bridge_free_ports(&gApp.ports);
    bridge_scan_processes(&gApp.procs);
    bridge_scan_ports(&gApp.ports);
    gApp.lastRefresh = GetTime();
}""",
     """static int sRefreshPending = 0;

static void RefreshData(void)
{
    /* 异步：后台线程扫描，完成后由主循环 poll 交付（避免卡帧） */
    if (bridge_scan_async_start())
        sRefreshPending = 1;
    gApp.lastRefresh = GetTime();
}

static void RefreshPoll(void)
{
    if (!sRefreshPending)
        return;
    BridgeProcList procs;
    BridgePortList ports;
    if (bridge_scan_async_poll(&procs, &ports)) {
        bridge_free_processes(&gApp.procs);
        bridge_free_ports(&gApp.ports);
        gApp.procs = procs;
        gApp.ports = ports;
        sRefreshPending = 0;
        RebuildViews();
        bridge_monitor_sync(&gApp.procs);
    }
}""", "async-refresh")

rep2("        DrawAiJobPoll();",
     "        RefreshPoll();\n        DrawAiJobPoll();", "poll-call")

# CLI /ai
rep2("""    if (strcmp(argv[1], "/kill") == 0 && argc >= 3) {""",
     """    if (strcmp(argv[1], "/ai") == 0 && argc >= 3 && strcmp(argv[2], "scan") == 0) {
        bridge_init();
        BridgeProcList pl;
        bridge_scan_processes(&pl);
        AiStartRiskScan(&pl);
        {
            int waited = 0;
            while (AiPoll() == 1 && waited < 300) {
                Sleep(1000);
                waited++;
            }
            if (AiPoll() == 2) {
                printf("%s\\n", AiGetResult());
                AiConsumeResult();
            } else {
                fprintf(stderr, "ai scan failed or timeout\\n");
            }
        }
        bridge_free_processes(&pl);
        return 0;
    }
    if (strcmp(argv[1], "/kill") == 0 && argc >= 3) {""", "cli-ai")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("main MISS:", miss2 if miss2 else "none")
