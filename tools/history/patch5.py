# patch5.py — 排序补全(9列)/端口日志排序/日志自动刷新/异常阈值可配置
import io

# ============ ui_views.c ============
P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss = []

def rep(old, new, tag, cnt=1):
    global t
    if old in t:
        t = t.replace(old, new, cnt)
    else:
        miss.append(tag)

# 1) 排序比较器扩展
rep("""static void SortFiltered(int col, int desc)
{
    int (*cmp)(const void *, const void *) = CmpProcMem;
    if (col == 1 || col == 2) cmp = CmpProcPid;
    else if (col == 0) cmp = CmpProcName;
    qsort(sFiltered, (size_t)sFilteredCount, sizeof(BridgeProc), cmp);
    if (desc && cmp == CmpProcName) {
        /* 名称降序反转 */
        for (int i = 0, j = sFilteredCount - 1; i < j; i++, j--) {
            BridgeProc t = sFiltered[i];
            sFiltered[i] = sFiltered[j];
            sFiltered[j] = t;
        }
    }
}""",
    """static int CmpProcPath(const void *a, const void *b)
{
    return strcmp(((const BridgeProc *)a)->path, ((const BridgeProc *)b)->path);
}

static int CmpProcCmd(const void *a, const void *b)
{
    return strcmp(((const BridgeProc *)a)->cmdline, ((const BridgeProc *)b)->cmdline);
}

static int CmpProcType(const void *a, const void *b)
{
    return (int)((const BridgeProc *)a)->type - (int)((const BridgeProc *)b)->type;
}

static int CmpProcCpu(const void *a, const void *b)
{
    float x = ((const BridgeProc *)a)->cpuPct, y = ((const BridgeProc *)b)->cpuPct;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

static int CmpProcRisk(const void *a, const void *b)
{
    return (int)((const BridgeProc *)a)->aiRisk - (int)((const BridgeProc *)b)->aiRisk;
}

static void SortFiltered(int col, int desc)
{
    int (*cmp)(const void *, const void *) = CmpProcMem;
    int reverse = 0;
    switch (col) {
    case 0: cmp = CmpProcName; reverse = 1; break;
    case 1: case 2: cmp = CmpProcPid; reverse = 1; break;
    case 4: cmp = CmpProcCpu; break;                /* CPU% 降序即默认 */
    case 5: cmp = CmpProcType; reverse = 1; break;
    case 6: cmp = CmpProcPath; reverse = 1; break;
    case 7: cmp = CmpProcCmd; reverse = 1; break;
    case 8: cmp = CmpProcRisk; break;               /* 风险高在前 */
    default: break;                                  /* 3=内存 默认降序 */
    }
    qsort(sFiltered, (size_t)sFilteredCount, sizeof(BridgeProc), cmp);
    if (desc && reverse) {
        for (int i = 0, j = sFilteredCount - 1; i < j; i++, j--) {
            BridgeProc t = sFiltered[i];
            sFiltered[i] = sFiltered[j];
            sFiltered[j] = t;
        }
    }
}""", "sort-9col")

# 2) 端口视图排序
rep("static int sortAllCol = 0, sortAllDesc = 0;\nstatic int selPortRow = -1;",
    "static int sortAllCol = 0, sortAllDesc = 0;\nstatic int sortPortCol = 0, sortPortDesc = 0;\nstatic int sortLogCol = 0, sortLogDesc = 0;\nstatic int selPortRow = -1;", "sortvars")

rep("""static void DrawViewPorts(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsPort, gCwPort, 7, NULL, NULL);
    int baseY = BeginList(x, y, w, h, (float)sPortRowCount, &gApp.scrollPort);""",
    """static int CmpPortRow(const void *a, const void *b)
{
    const PortRow *x = a, *y = b;
    switch (sortPortCol) {
    case 0: return (x->port < y->port) ? -1 : (x->port > y->port) ? 1 : 0;
    case 2: return (x->pid < y->pid) ? -1 : (x->pid > y->pid) ? 1 : 0;
    case 3: return strcmp(x->p ? x->p->name : "", y->p ? y->p->name : "");
    default: return 0;
    }
}

static void DrawViewPorts(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsPort, gCwPort, 7, &sortPortCol, &sortPortDesc);
    /* 排序快照（每次进入按当前排序状态重排行序） */
    qsort(sPortRows, (size_t)sPortRowCount, sizeof(PortRow), CmpPortRow);
    int baseY = BeginList(x, y, w, h, (float)sPortRowCount, &gApp.scrollPort);""", "ports-sort")

# 3) 日志视图排序
rep("""static void DrawViewLogs(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsLog, gCwLog, 6, NULL, NULL);""",
    """static int CmpLogRow(const void *a, const void *b)
{
    const BridgeLog *x = a, *y = b;
    switch (sortLogCol) {
    case 0: return strcmp(x->timeText, y->timeText);
    case 2: return strcmp(x->name, y->name);
    case 4: return x->ok - y->ok;
    default: return 0;
    }
}

static void DrawViewLogs(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsLog, gCwLog, 6, &sortLogCol, &sortLogDesc);
    qsort(gApp.logs.items, gApp.logs.count, sizeof(BridgeLog), CmpLogRow);""", "logs-sort")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("ui_views MISS:", miss if miss else "none")

# ============ main.c: 日志自动刷新 + 异常阈值 ============
P2 = r"src\main.c"
with io.open(P2, encoding="utf-8") as f:
    t2 = f.read()
miss2 = []

def rep2(old, new, tag):
    global t2
    if old in t2:
        t2 = t2.replace(old, new, 1)
    else:
        miss2.append(tag)

rep2("""void MainUiRefresh(void)
{
    RefreshData();
    RebuildViews();
}""",
     """void MainUiRefresh(void)
{
    RefreshData();
    RebuildViews();
    /* 日志页签时同步刷新日志 */
    if (gApp.curTab == TAB_LOGS) {
        bridge_free_logs(&gApp.logs);
        bridge_load_logs(&gApp.logs);
    }
}""", "log-autorefresh")

rep2("""                if (bridge_monitor_mem_growth(p->pid, &growth)) {""",
     """                if (gApp.anomalyMemMB > 0 && growth >= (unsigned long long)gApp.anomalyMemMB) {""", "anomaly-cfg")

rep2("""    gApp.balloonNotify = bridge_config_bool("BalloonNotify", 1);""",
     """    gApp.balloonNotify = bridge_config_bool("BalloonNotify", 1);
    gApp.anomalyMemMB = (int)bridge_config_long("AnomalyMemMB", 10);""", "anomaly-load")

rep2("""                unsigned long long growth = 0;
                if (p->type == 0) continue;
                if (gApp.anomalyMemMB > 0 && growth >= (unsigned long long)gApp.anomalyMemMB) {""",
     """                unsigned long long growth = 0;
                if (p->type == 0) continue;
                if (bridge_monitor_mem_growth(p->pid, &growth) &&
                    growth >= (unsigned long long)gApp.anomalyMemMB) {""", "anomaly-fix")

with io.open(P2, "w", encoding="utf-8", newline="") as f:
    f.write(t2)
print("main MISS:", miss2 if miss2 else "none")

# ============ AppState 加 anomalyMemMB ============
P3 = r"src\app_shared.h"
with io.open(P3, encoding="utf-8") as f:
    t3 = f.read()
if "anomalyMemMB" not in t3:
    t3 = t3.replace("    int anomalyWatch;", "    int anomalyWatch;\n    int anomalyMemMB;   /* 异常告警内存阈值 MB */")
    with io.open(P3, "w", encoding="utf-8", newline="") as f:
        f.write(t3)
    print("state field added")
else:
    print("state field already")
