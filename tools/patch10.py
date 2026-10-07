# patch10.py — ui_views: 双击详情/导出CSV/统计按钮/右键项 + main.c modal分发
import io

# ============ ui_views.h ============
P = r"src\ui_views.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "ExportCsv" not in t:
    t = t.replace("void BaselineCompare(void);",
                  "void BaselineCompare(void);\nvoid ExportProcessesCsv(void); /* 全部进程导出 CSV */")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("hdr ok")

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

# 1) 双击检测 helper（在 ProcRowInput 附近加）
rep("""/* 返回是否命中右键 */
static int ProcRowInput(Rectangle rowR, const BridgeProc *p)
{
    if (!PtIn(rowR))
        return 0;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        gApp.selectedPid = (int)p->pid;""",
"""/* 双击检测（300ms 内同行两次左键） */
static int IsDoubleClickOn(Rectangle r)
{
    static double lastClick = 0;
    static Vector2 lastPos = {0, 0};
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && PtIn(r)) {
        double now = GetTime();
        Vector2 m = GetMousePosition();
        if (now - lastClick < 0.3 && fabsf(m.x - lastPos.x) < 6 &&
            fabsf(m.y - lastPos.y) < 6) {
            lastClick = 0;
            return 1;
        }
        lastClick = now;
        lastPos = m;
    }
    return 0;
}

/* 返回是否命中右键 */
static int ProcRowInput(Rectangle rowR, const BridgeProc *p)
{
    if (!PtIn(rowR))
        return 0;
    if (IsDoubleClickOn(rowR)) {
        extern void OpenProcDetail(unsigned long pid);
        OpenProcDetail(p->pid);
        return 0;
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        gApp.selectedPid = (int)p->pid;""", "dblclick")

# fabsf 需要 math.h
if "#include <math.h>" not in t:
    rep("#include <stdlib.h>", "#include <stdlib.h>\n#include <math.h>", "math-h")

# 2) 右键菜单：进程行加“进程详情 / 导出 CSV”（7 项 -> 9 项）
rep("""    int itemCount = gApp.menuKind == 0 ? 7 : 3;
    float mw = 460;""",
    """    int itemCount = gApp.menuKind == 0 ? 9 : 3;
    float mw = 460;""", "menu-count")
rep("""    if (gApp.menuKind == 0) {
        if (MenuItem(M_KILL, &mr, mx, &y, mw)) {""",
    """    if (gApp.menuKind == 0) {
        if (MenuItem(M_DETAIL, &mr, mx, &y, mw)) {
            extern void OpenProcDetail(unsigned long pid);
            OpenProcDetail((unsigned long)gApp.selectedPid);
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_EXPORT_CSV, &mr, mx, &y, mw)) {
            ExportProcessesCsv();
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_KILL, &mr, mx, &y, mw)) {""", "menu-items")

# 3) ExportProcessesCsv 实现（文件尾追加）
csv_impl = '''
/* ---------- 导出进程 CSV ---------- */

void ExportProcessesCsv(void)
{
    FILE *f = fopen("processes_export.csv", "wb");
    if (!f) {
        SetFlashMsg("CSV 导出失败（无法写入文件）");
        return;
    }
    /* UTF-8 BOM（Excel 兼容） */
    fwrite("\\xef\\xbb\\xbf", 1, 3, f);
    fprintf(f, "进程名,PID,父PID,内存MB,CPU%%,类型,AI风险,可执行路径,命令行,项目\\n");
    for (size_t i = 0; i < gApp.procs.count; i++) {
        BridgeProc *p = &gApp.procs.items[i];
        char pathEsc[600], cmdEsc[600], projEsc[300];
        snprintf(pathEsc, sizeof(pathEsc), "%s", p->path);
        snprintf(cmdEsc, sizeof(cmdEsc), "%s", p->cmdline);
        snprintf(projEsc, sizeof(projEsc), "%s", p->project);
        /* 逗号引号转义：包双引号，内部 " 翻倍 */
        for (char *q = pathEsc; *q; q++) if (*q == '"') { memmove(q + 1, q, strlen(q)); *q = '"'; q++; }
        for (char *q = cmdEsc; *q; q++) if (*q == '"') { memmove(q + 1, q, strlen(q)); *q = '"'; q++; }
        for (char *q = projEsc; *q; q++) if (*q == '"') { memmove(q + 1, q, strlen(q)); *q = '"'; q++; }
        fprintf(f, "\\"%s\\",%lu,%lu,%.1f,%.1f,\\"%s\\",%d,\\"%s\\",\\"%s\\",\\"%s\\"\\n",
                p->name, (unsigned long)p->pid, (unsigned long)p->ppid,
                (double)p->memBytes / 1048576.0,
                (double)(p->cpuPct < 0 ? 0 : p->cpuPct),
                ProcTypeName(p->type), p->aiRisk, pathEsc, cmdEsc, projEsc);
    }
    fclose(f);
    SetFlashMsg("已导出 processes_export.csv（程序目录）");
}
'''
if "ExportProcessesCsv" not in t:
    t = t + "\n" + csv_impl

# 4) 日志页签“查看统计”按钮 + 统计入口
rep("""static void DrawViewLogs(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsLog, gCwLog, 6, &sortLogCol, &sortLogDesc);""",
    """static void DrawViewLogs(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsLog, gCwLog, 6, &sortLogCol, &sortLogDesc);
    if (DrawTextButton("终止统计", (Rectangle){x + w - 130, y - 2, 120, 40}, 1)) {
        extern void BuildKillStats(void);
        BuildKillStats();
        gApp.modal = 4;
    }""", "logs-stats-btn")

# 5) ui_text.h 标签
P3 = r"src\ui_text.h"
with io.open(P3, encoding="utf-8") as f:
    t3 = f.read()
if "M_DETAIL" not in t3:
    t3 = t3.replace("#define M_KILL      " + '"终止选中"',
                    "#define M_DETAIL    " + '"进程详情"\n#define M_EXPORT_CSV ' + '"导出全部CSV"\n#define M_KILL      "终止选中"')
    with io.open(P3, "w", encoding="utf-8", newline="") as f:
        f.write(t3)
    print("labels ok")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("ui_views MISS:", miss if miss else "none")

# ============ main.c: modal 3/4 ============
P = r"src\main.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "DrawProcDetailModal" not in t:
    t = t.replace("""        else if (gApp.modal == 2)
            DrawAiPanel();""",
"""        else if (gApp.modal == 2)
            DrawAiPanel();
        else if (gApp.modal == 3)
            DrawProcDetailModal();
        else if (gApp.modal == 4)
            DrawStatsModal();""")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("main modal ok")

# app_shared.h 声明 OpenProcDetail/DrawProcDetailModal/DrawStatsModal
P = r"src\app_shared.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
added = False
if "OpenProcDetail" not in t:
    t = t.replace("void UiOnColumnResize(void);            /* 列宽拖拽结束回调（写配置） */",
                  "void UiOnColumnResize(void);            /* 列宽拖拽结束回调（写配置） */\nvoid OpenProcDetail(unsigned long pid);  /* 打开进程详情（modal=3） */")
    added = True
if "DrawStatsModal" not in t:
    t = t.replace("void AiApplyCleanStrategy(void);        /* 按清理策略 JSON 终止进程 */",
                  "void AiApplyCleanStrategy(void);        /* 按清理策略 JSON 终止进程 */\nvoid DrawProcDetailModal(void);         /* 进程详情弹窗 */\nvoid DrawStatsModal(void);              /* 终止历史统计弹窗 */\nvoid BuildKillStats(void);              /* 聚合日志统计 */")
    added = True
with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("hdr added:", added)
