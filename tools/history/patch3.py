# patch3.py — ui_views CPU列/树折叠/列宽持久化 + ai_bridge AiParseCleanJson
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

# 1) ALL 视图列定义插入 CPU 列（内存后）
rep("static const char *gColsAll[] = {C_NAME, C_PID, C_PPID, C_MEM, C_TYPE, C_PATH, C_CMDLINE, C_RISK};",
    "static const char *gColsAll[] = {C_NAME, C_PID, C_PPID, C_MEM, C_CPU, C_TYPE, C_PATH, C_CMDLINE, C_RISK};",
    "cols-cpu")
rep("static float gCwAll[] = {236, 80, 80, 110, 90, 270, 300, 70};",
    "static float gCwAll[] = {220, 80, 80, 110, 80, 90, 230, 240, 70};",
    "cw-cpu")

# 2) 折叠集合 + 存取（静态数据区末尾，selPortRow 之后）
anchor = "static int *sSortCol = &sortAllCol;\nstatic int *sSortDesc = &sortAllDesc;"
fold = anchor + '''

/* 树形折叠状态（按 PID） */
static unsigned long sFolded[512];
static int sFoldedCount = 0;

static int FoldIsOn(unsigned long pid)
{
    for (int i = 0; i < sFoldedCount; i++)
        if (sFolded[i] == pid) return 1;
    return 0;
}

static void FoldToggle(unsigned long pid)
{
    for (int i = 0; i < sFoldedCount; i++)
        if (sFolded[i] == pid) {
            sFolded[i] = sFolded[--sFoldedCount];
            return;
        }
    if (sFoldedCount < 512)
        sFolded[sFoldedCount++] = pid;
}'''
if "FoldIsOn" not in t:
    rep(anchor, fold, "fold-block")

# 3) TreeDfs 折叠剪枝：折叠节点只输出自身
rep("""static int TreeDfs(int node, int depth, int *outPos)
{
    if (sNodes[node].visited) return *outPos;
    sNodes[node].visited = 1;
    int myPos = (*outPos)++;
    gApp.treeDepth[myPos] = depth;
    gApp.treeMem[myPos] = sFiltered[sNodes[node].idx].memBytes;
    gApp.treeProcs.items[myPos] = sFiltered[sNodes[node].idx];
    unsigned long long sub = 0;
    for (int k = 0; k < sNodes[node].childCount; k++)
        sub += (unsigned long long)TreeDfs(sKids[sNodes[node].firstChild + k],
                                           depth + 1, outPos);
    /* 子树合计 = 自身 + 子树 */
    gApp.treeMem[myPos] = sFiltered[sNodes[node].idx].memBytes + sub;
    return myPos;
}""",
    """static int TreeDfs(int node, int depth, int *outPos)
{
    if (sNodes[node].visited) return *outPos;
    sNodes[node].visited = 1;
    int myPos = (*outPos)++;
    gApp.treeDepth[myPos] = depth;
    gApp.treeMem[myPos] = sFiltered[sNodes[node].idx].memBytes;
    gApp.treeProcs.items[myPos] = sFiltered[sNodes[node].idx];
    gApp.treeHasKids[myPos] = sNodes[node].childCount > 0;
    unsigned long long sub = 0;
    int folded = FoldIsOn(sFiltered[sNodes[node].idx].pid);
    if (!folded) {
        for (int k = 0; k < sNodes[node].childCount; k++)
            sub += (unsigned long long)TreeDfs(sKids[sNodes[node].firstChild + k],
                                               depth + 1, outPos);
    }
    /* 子树合计 = 自身 + 子树 */
    gApp.treeMem[myPos] = sFiltered[sNodes[node].idx].memBytes + sub;
    return myPos;
}""", "treedfs-fold")

# 4) treeHasKids 数组分配与清理
rep("""    free(gApp.treeDepth); gApp.treeDepth = NULL;
    free(gApp.treeMem); gApp.treeMem = NULL;
    free(sNodes); sNodes = NULL;
    free(sKids); sKids = NULL;
    sNodeCount = 0;
    gApp.treeCount = 0;""",
    """    free(gApp.treeDepth); gApp.treeDepth = NULL;
    free(gApp.treeMem); gApp.treeMem = NULL;
    free(gApp.treeHasKids); gApp.treeHasKids = NULL;
    free(sNodes); sNodes = NULL;
    free(sKids); sKids = NULL;
    sNodeCount = 0;
    gApp.treeCount = 0;""", "tree-free")
rep("""    gApp.treeDepth = (int *)calloc((size_t)n, sizeof(int));
    gApp.treeMem = (unsigned long long *)calloc((size_t)n, sizeof(unsigned long long));
    gApp.treeProcs.items = (BridgeProc *)calloc((size_t)n, sizeof(BridgeProc));
    sKids = (int *)calloc((size_t)n, sizeof(int));
    if (!sNodes || !gApp.treeDepth || !gApp.treeMem || !gApp.treeProcs.items || !sKids) {""",
    """    gApp.treeDepth = (int *)calloc((size_t)n, sizeof(int));
    gApp.treeMem = (unsigned long long *)calloc((size_t)n, sizeof(unsigned long long));
    gApp.treeHasKids = (int *)calloc((size_t)n, sizeof(int));
    gApp.treeProcs.items = (BridgeProc *)calloc((size_t)n, sizeof(BridgeProc));
    sKids = (int *)calloc((size_t)n, sizeof(int));
    if (!sNodes || !gApp.treeDepth || !gApp.treeMem || !gApp.treeHasKids ||
        !gApp.treeProcs.items || !sKids) {""", "tree-alloc")

# 5) BuildProcCells：非树 9 列插 CPU；树形加 ▸▾ 前缀
old_cells = """    snprintf(cells[0], 220, "%s", p->name);
    snprintf(cells[1], 220, "%lu", (unsigned long)p->pid);
    snprintf(cells[2], 220, "%lu", (unsigned long)p->ppid);
    FormatMem(memOverride ? memOverride : p->memBytes, cells[3], 220);
    snprintf(cells[4], 220, "%s", ProcTypeName(p->type));
    snprintf(cells[5], 220, "%s", p->path[0] ? p->path : P_NOREAD);
    snprintf(cells[6], 220, "%s", p->cmdline[0] ? p->cmdline : "-");
    snprintf(cells[7], 220, "%s",
             p->aiRisk ? (p->aiRisk >= 3 ? "高" : p->aiRisk == 2 ? "中" : "低") : "-");
    return isTree ? 6 : 8;"""
new_cells = """    snprintf(cells[0], 220, "%s", p->name);
    snprintf(cells[1], 220, "%lu", (unsigned long)p->pid);
    snprintf(cells[2], 220, "%lu", (unsigned long)p->ppid);
    FormatMem(memOverride ? memOverride : p->memBytes, cells[3], 220);
    if (p->cpuPct >= 0)
        snprintf(cells[4], 220, "%.1f%%", (double)p->cpuPct);
    else
        snprintf(cells[4], 220, "-");
    snprintf(cells[5], 220, "%s", ProcTypeName(p->type));
    snprintf(cells[6], 220, "%s", p->path[0] ? p->path : P_NOREAD);
    snprintf(cells[7], 220, "%s", p->cmdline[0] ? p->cmdline : "-");
    snprintf(cells[8], 220, "%s",
             p->aiRisk ? (p->aiRisk >= 3 ? "高" : p->aiRisk == 2 ? "中" : "低") : "-");
    return isTree ? 6 : 9;"""
rep(old_cells, new_cells, "cells-cpu")
# 树形前缀 ▸▾（childCount>0 时）：给 BuildProcCells 加 hasKids 参数
rep("static int BuildProcCells(const BridgeProc *p, char cells[][220], int indent,\n                          int isTree, unsigned long long memOverride)",
    "static int BuildProcCells(const BridgeProc *p, char cells[][220], int indent,\n                          int isTree, unsigned long long memOverride, int hasKids)",
    "cells-sig")
rep("""        if (indent > 0)
            snprintf(cells[0], 220, "%*s%s %s", indent * 2, "",
                     indent == 1 ? "└" : "│", p->name);
        else
            snprintf(cells[0], 220, "%s", p->name);""",
    """        const char *arrow = hasKids ? (FoldIsOn(p->pid) ? "▸ " : "▾ ") : "";
        if (indent > 0)
            snprintf(cells[0], 220, "%*s%s%s%s", indent * 2, "",
                     indent == 1 ? "└" : "│", arrow, p->name);
        else
            snprintf(cells[0], 220, "%s%s", arrow, p->name);""", "cells-arrow")

# 6) 调用点签名更新 + CPU 填充 + 树折叠点击
rep("int ncols = BuildProcCells(p, cells, indent, isTree, memOverride);\n    ProcRowColors(p, colors, ncols);\n    if (!isTree && p->aiRisk >= 3 && ncols >= 8)\n        colors[7] = gPal.errorC;",
    "int ncols = BuildProcCells(p, cells, indent, isTree, memOverride, hasKids);\n    ProcRowColors(p, colors, ncols);\n    if (ncols >= 9) {\n        colors[4] = p->cpuPct > 50.0f ? gPal.errorC\n                    : p->cpuPct > 20.0f ? gPal.warnC\n                    : gPal.onSurfaceVariant;\n    }\n    if (!isTree && p->aiRisk >= 3 && ncols >= 9)\n        colors[8] = gPal.errorC;", "common-call")
rep("static void DrawProcRowCommon(const BridgeProc *p, float x, float rowY,\n                              float *cw, int indent, int isTree,\n                              unsigned long long memOverride)",
    "static void DrawProcRowCommon(const BridgeProc *p, float x, float rowY,\n                              float *cw, int indent, int isTree,\n                              unsigned long long memOverride, int hasKids)", "common-sig")

# CPU 列颜色循环（DrawViewAll 内）：ncols 9 时 colors[4] 特殊
rep("""        int ncols = BuildProcCells(p, cells, 0, 0, 0);
        ProcRowColors(p, colors, ncols);
        if (p->aiRisk >= 3 && ncols >= 8)
            colors[7] = gPal.errorC;
        DrawRowIcon(p, x, rowY);
        DrawRowCells(cells, colors, gCwAll, ncols, x, rowY, 1);""",
    """        int ncols = BuildProcCells(p, cells, 0, 0, 0, 0);
        ProcRowColors(p, colors, ncols);
        if (ncols >= 9) {
            colors[4] = p->cpuPct > 50.0f ? gPal.errorC
                        : p->cpuPct > 20.0f ? gPal.warnC
                        : gPal.onSurfaceVariant;
        }
        if (p->aiRisk >= 3 && ncols >= 9)
            colors[8] = gPal.errorC;
        DrawRowIcon(p, x, rowY);
        DrawRowCells(cells, colors, gCwAll, ncols, x, rowY, 1);""", "viewall-cpu")
rep("        int ncols = BuildProcCells(p, cells, 0, 0, 0, 0);\n        ProcRowColors(p, colors, ncols);",
    "        int ncols = BuildProcCells(p, cells, 0, 0, 0, 0);\n        ProcRowColors(p, colors, ncols);", "noop")

rep("        int ncols = BuildProcCells(p, cells, gApp.treeDepth[i], 1, gApp.treeMem[i]);",
    "        int ncols = BuildProcCells(p, cells, gApp.treeDepth[i], 1, gApp.treeMem[i],\n                                   gApp.treeHasKids[i]);", "tree-call")

# 7) 树视图：点击首列图标/箭头区折叠切换
rep("""        Rectangle rowR = {x, rowY, w, ROW_H};
        if (ProcRowInput(rowR, p)) break;
        int ncols = BuildProcCells(p, cells, gApp.treeDepth[i], 1, gApp.treeMem[i],
                                   gApp.treeHasKids[i]);""",
    """        Rectangle rowR = {x, rowY, w, ROW_H};
        Rectangle arrowZone = {x, rowY, 34 + gApp.treeDepth[i] * 18, ROW_H};
        if (PtIn(arrowZone) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            gApp.treeHasKids[i]) {
            FoldToggle(p->pid);
            RebuildTree();
            continue;
        }
        if (ProcRowInput(rowR, p)) break;
        int ncols = BuildProcCells(p, cells, gApp.treeDepth[i], 1, gApp.treeMem[i],
                                   gApp.treeHasKids[i]);""", "tree-foldclick")

# 8) CPU 填充：RebuildViews 排序后填充 sFiltered[].cpuPct
rep("""    /* 2) 排序 */
    if (gApp.curTab == TAB_ALL) { sSortCol = &sortAllCol; sSortDesc = &sortAllDesc; }
    SortFiltered(*sSortCol, *sSortDesc);""",
    """    /* 2) 排序 */
    if (gApp.curTab == TAB_ALL) { sSortCol = &sortAllCol; sSortDesc = &sortAllDesc; }
    SortFiltered(*sSortCol, *sSortDesc);
    /* 2.5) CPU% 采样填充 */
    for (int i = 0; i < sFilteredCount; i++)
        sFiltered[i].cpuPct = bridge_monitor_cpu(sFiltered[i].pid);""", "cpu-fill")

# 9) 列宽持久化：UiOnColumnResize + ViewsInit 读回
impl = '''
void UiOnColumnResize(void)
{
    for (int i = 0; i < 9; i++) {
        char key[16];
        snprintf(key, sizeof(key), "cwAll%d", i);
        bridge_config_set_long(key, (long)gCwAll[i]);
    }
}
'''
if "void UiOnColumnResize(void)" not in t:
    t = t.replace("void ViewsInit(void)\n{", impl + "\nvoid ViewsInit(void)\n{")
rep("""    gApp.reservedRanges = rl.items;
    gApp.reservedCount = (int)rl.count;
    RebuildViews();""",
    """    gApp.reservedRanges = rl.items;
    gApp.reservedCount = (int)rl.count;
    for (int i = 0; i < 9; i++) {
        char key[16];
        snprintf(key, sizeof(key), "cwAll%d", i);
        long v = bridge_config_long(key, 0);
        if (v >= 56 && v <= 900) gCwAll[i] = (float)v;
    }
    RebuildViews();""", "cw-restore")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("ui_views MISS:", miss if miss else "none")

# ============ ai_bridge.c: AiParseCleanJson ============
P2 = r"src\ai_bridge.c"
with io.open(P2, encoding="utf-8") as f:
    t2 = f.read()
if "AiParseCleanJson" not in t2:
    fn = '''
int AiParseCleanJson(const char *text, unsigned int *pids, int max)
{
    const char *p = text;
    int n = 0;
    while (n < max && (p = strstr(p, "\\"pid\\"")) != NULL) {
        const char *q = strchr(p, ':');
        if (!q) break;
        unsigned long pid = strtoul(q + 1, NULL, 10);
        p = q + 1;
        if (pid > 0)
            pids[n++] = (unsigned int)pid;
    }
    return n;
}
'''
    with io.open(P2, "w", encoding="utf-8", newline="") as f:
        f.write(t2 + fn)
    print("cleanjson added")
else:
    print("cleanjson already")
