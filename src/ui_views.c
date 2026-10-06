/* ui_views.c — 7 视图 + 右键菜单 + 设置弹窗 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "raylib.h"
#include "app_shared.h"
#include "ui_text.h"
#include "ai_bridge.h"
#include "sys_bridge.h"

/* ---------- 静态数据 ---------- */

static BridgeProc *sFiltered = NULL;
static int sFilteredCount = 0;
static int sFilteredCap = 0;

typedef struct {
    int isRange;
    unsigned long port, portEnd;
    unsigned long pid;
    int tcp;
    BridgeProc *p;
} PortRow;
static PortRow *sPortRows = NULL;
static int sPortRowCount = 0, sPortRowCap = 0;

static int sortAllCol = 0, sortAllDesc = 0;
static int selPortRow = -1;

/* 排序状态存在 AppState 外部即可（每视图独立） */
static int *sSortCol = &sortAllCol;
static int *sSortDesc = &sortAllDesc;

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
}

/* ---------- 工具 ---------- */

static int PtIn(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static int NameMatch(const BridgeProc *p)
{
    if (!gApp.filterLen)
        return 1;
    /* 大小写不敏感子串匹配 name/path/project */
    char hay[600];
    snprintf(hay, sizeof(hay), "%s %s %s", p->name, p->path, p->project);
    for (char *q = hay; *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q += 32;
    char needle[256];
    snprintf(needle, sizeof(needle), "%s", gApp.filterBuf);
    for (char *q = needle; *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q += 32;
    return strstr(hay, needle) != NULL;
}

static BridgeProc *FindPid(unsigned long pid)
{
    for (size_t i = 0; i < gApp.procs.count; i++)
        if (gApp.procs.items[i].pid == pid)
            return &gApp.procs.items[i];
    return NULL;
}

/* ---------- 重建视图 ---------- */

static int CmpProcMem(const void *a, const void *b)
{
    const BridgeProc *x = a, *y = b;
    return (x->memBytes < y->memBytes) ? 1 : (x->memBytes > y->memBytes) ? -1 : 0;
}

static int CmpProcPid(const void *a, const void *b)
{
    const BridgeProc *x = a, *y = b;
    return (int)x->pid - (int)y->pid;
}

static int CmpProcName(const void *a, const void *b)
{
    return strcmp(((const BridgeProc *)a)->name, ((const BridgeProc *)b)->name);
}

static void SortFiltered(int col, int desc)
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
}

/* 树构建：DFS，输出顺序数组 + 深度 + 子树内存 */
typedef struct {
    int idx;              /* sFiltered 索引 */
    int firstChild, childCount;
    int visited;
} TreeNode;

static TreeNode *sNodes = NULL;
static int *sKids = NULL;        /* 子索引表（按父聚合） */
static int sNodeCount = 0;

static void TreeFree(void)
{
    free(gApp.treeDepth); gApp.treeDepth = NULL;
    free(gApp.treeMem); gApp.treeMem = NULL;
    free(gApp.treeHasKids); gApp.treeHasKids = NULL;
    free(sNodes); sNodes = NULL;
    free(sKids); sKids = NULL;
    sNodeCount = 0;
    gApp.treeCount = 0;
}

static int TreeDfs(int node, int depth, int *outPos)
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
}

static void RebuildTree(void)
{
    TreeFree();
    int n = sFilteredCount;
    if (n == 0) return;
    sNodes = (TreeNode *)calloc((size_t)n, sizeof(TreeNode));
    gApp.treeDepth = (int *)calloc((size_t)n, sizeof(int));
    gApp.treeMem = (unsigned long long *)calloc((size_t)n, sizeof(unsigned long long));
    gApp.treeHasKids = (int *)calloc((size_t)n, sizeof(int));
    gApp.treeProcs.items = (BridgeProc *)calloc((size_t)n, sizeof(BridgeProc));
    sKids = (int *)calloc((size_t)n, sizeof(int));
    if (!sNodes || !gApp.treeDepth || !gApp.treeMem || !gApp.treeHasKids ||
        !gApp.treeProcs.items || !sKids) {
        TreeFree();
        return;
    }
    sNodeCount = n;
    gApp.treeProcs.count = n;

    for (int i = 0; i < n; i++) {
        sNodes[i].idx = i;
        sNodes[i].firstChild = -1;
    }
    /* pid -> node 索引（线性查，进程几百个可接受） */
    int *parent = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) {
        BridgeProc *p = &sFiltered[i];
        parent[i] = -1;
        for (int j = 0; j < n; j++) {
            if (j != i && sFiltered[j].pid == p->ppid) {
                parent[i] = j;
                break;
            }
        }
    }
    /* 聚合子表 */
    int kidCount = 0;
    for (int i = 0; i < n; i++)
        if (parent[i] >= 0) sNodes[parent[i]].childCount++;
    for (int i = 0; i < n; i++) {
        if (sNodes[i].childCount > 0) {
            sNodes[i].firstChild = kidCount;
            kidCount += sNodes[i].childCount;
            sNodes[i].childCount = 0;
        }
    }
    for (int i = 0; i < n; i++) {
        if (parent[i] >= 0) {
            TreeNode *pn = &sNodes[parent[i]];
            sKids[pn->firstChild + pn->childCount++] = i;
        }
    }
    free(parent);

    int outPos = 0;
    for (int i = 0; i < n; i++)
        if (parent[i] < 0)
            TreeDfs(i, 0, &outPos);
    /* 处理环（visited 未覆盖的节点） */
    for (int i = 0; i < n; i++)
        if (!sNodes[i].visited)
            TreeDfs(i, 0, &outPos);
    gApp.treeCount = outPos;
}

static void RebuildProject(void)
{
    free(gApp.projProcs.items); gApp.projProcs.items = NULL;
    free(gApp.projGroupStart); gApp.projGroupStart = NULL;
    free(gApp.projGroupName); gApp.projGroupName = NULL;
    gApp.projGroupCount = 0;
    gApp.projRowCount = 0;

    int n = sFilteredCount;
    if (n == 0) return;
    /* 排序副本：按 project 分组（空最后），组内内存降序 */
    BridgeProc *arr = (BridgeProc *)malloc((size_t)n * sizeof(BridgeProc));
    memcpy(arr, sFiltered, (size_t)n * sizeof(BridgeProc));

    /* 简单排序：project 名升序（空串排最后），组内内存降序 */
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            const char *a = arr[j].project[0] ? arr[j].project : "\xFF";
            const char *b = arr[j + 1].project[0] ? arr[j + 1].project : "\xFF";
            int c = strcmp(a, b);
            int swap = c > 0;
            if (c == 0 && arr[j].memBytes < arr[j + 1].memBytes)
                swap = 1;
            if (swap) {
                BridgeProc t = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = t;
            }
        }
    }

    gApp.projProcs.items = arr;
    gApp.projProcs.count = n;

    /* 分组统计 */
    int groups = 0;
    const char *last = NULL;
    for (int i = 0; i < n; i++) {
        const char *g = arr[i].project[0] ? arr[i].project : "";
        if (!last || strcmp(g, last) != 0) {
            groups++;
            last = g;
        }
    }
    gApp.projGroupCount = groups;
    gApp.projGroupStart = (int *)calloc((size_t)groups, sizeof(int));
    gApp.projGroupName = (char(*)[260])calloc((size_t)groups, 260);
    gApp.projRowCount = n + groups;

    int gi = -1;
    last = NULL;
    for (int i = 0; i < n; i++) {
        const char *g = arr[i].project[0] ? arr[i].project : "";
        if (!last || strcmp(g, last) != 0) {
            gi++;
            gApp.projGroupStart[gi] = i + gi; /* +组头行偏移 */
            snprintf(gApp.projGroupName[gi], 260, "%s",
                     arr[i].project[0] ? arr[i].project : P_NOPROJ);
            last = g;
        }
    }
}

void RebuildViews(void)
{
    /* 1) 过滤 */
    int cap = (int)gApp.procs.count;
    if (cap > sFilteredCap) {
        free(sFiltered);
        sFilteredCap = cap + 64;
        sFiltered = (BridgeProc *)malloc((size_t)sFilteredCap * sizeof(BridgeProc));
    }
    sFilteredCount = 0;
    for (size_t i = 0; i < gApp.procs.count; i++) {
        BridgeProc *p = &gApp.procs.items[i];
        if (gApp.curTab == TAB_NODEPY && p->type == 0)
            continue;
        if (!NameMatch(p))
            continue;
        sFiltered[sFilteredCount++] = *p;
    }

    /* 2) 排序 */
    if (gApp.curTab == TAB_ALL) { sSortCol = &sortAllCol; sSortDesc = &sortAllDesc; }
    SortFiltered(*sSortCol, *sSortDesc);
    /* 2.5) CPU% 采样填充 */
    for (int i = 0; i < sFilteredCount; i++)
        sFiltered[i].cpuPct = bridge_monitor_cpu(sFiltered[i].pid);

    /* 3) 树/项目 */
    RebuildTree();
    RebuildProject();

    /* 4) 端口行（含保留区间） */
    int total = (int)gApp.ports.count + gApp.reservedCount;
    if (total > sPortRowCap) {
        free(sPortRows);
        sPortRowCap = total + 64;
        sPortRows = (PortRow *)malloc((size_t)sPortRowCap * sizeof(PortRow));
    }
    sPortRowCount = 0;
    for (size_t i = 0; i < gApp.ports.count; i++) {
        BridgePort *bp = &gApp.ports.items[i];
        PortRow *r = &sPortRows[sPortRowCount++];
        memset(r, 0, sizeof(*r));
        r->port = bp->port;
        r->pid = bp->pid;
        r->tcp = bp->tcp;
        r->p = FindPid(bp->pid);
    }
    for (int i = 0; i < gApp.reservedCount; i++) {
        BridgeRange *rg = &((BridgeRange *)gApp.reservedRanges)[i];
        PortRow *r = &sPortRows[sPortRowCount++];
        memset(r, 0, sizeof(*r));
        r->isRange = 1;
        r->port = rg->start;
        r->portEnd = rg->end;
        r->tcp = rg->tcp;
    }

    /* 5) 状态栏 */
    int nodeN = 0, pyN = 0;
    for (size_t i = 0; i < gApp.procs.count; i++) {
        if (gApp.procs.items[i].type == 1) nodeN++;
        else if (gApp.procs.items[i].type == 2) pyN++;
    }
    snprintf(gApp.statusText, sizeof(gApp.statusText), T_FMT_STATUS,
             (int)gApp.procs.count, nodeN, pyN, (int)gApp.ports.count);
}



/* ---------- 视图绘制 ---------- */



static const char *gColsAll[] = {C_NAME, C_PID, C_PPID, C_MEM, C_CPU, C_TYPE, C_PATH, C_CMDLINE, C_RISK};
static float gCwAll[] = {220, 80, 80, 110, 80, 90, 230, 240, 70};
static const char *gColsTree[] = {C_TREE, C_PID, C_PPID, C_MEM, C_TYPE, C_PATH};
static float gCwTree[] = {376, 90, 90, 120, 90, 420};
static const char *gColsProj[] = {C_PROJECT, C_NAME, C_PID, C_MEM, C_TYPE, C_CMDLINE};
static float gCwProj[] = {260, 200, 80, 110, 90, 390};
static const char *gColsPort[] = {C_PORT, C_PROTO, C_PID, C_NAME, C_TYPE, C_MEM, C_PATH};
static float gCwPort[] = {100, 80, 90, 190, 90, 120, 400};
static const char *gColsLog[] = {C_TIME, C_SOURCE, C_NAME, C_PID, C_RESULT, C_PATH};
static float gCwLog[] = {190, 100, 190, 90, 80, 400};

void UiOnColumnResize(void)
{
    for (int i = 0; i < 9; i++) {
        char key[16];
        snprintf(key, sizeof(key), "cwAll%d", i);
        bridge_config_set_long(key, (long)gCwAll[i]);
    }
}

void ViewsInit(void)
{
    gApp.reservedRanges = NULL;
    gApp.reservedCount = 0;
    BridgeRangeList rl;
    bridge_scan_reserved(&rl);
    gApp.reservedRanges = rl.items;
    gApp.reservedCount = (int)rl.count;
    for (int i = 0; i < 9; i++) {
        char key[16];
        snprintf(key, sizeof(key), "cwAll%d", i);
        long v = bridge_config_long(key, 0);
        if (v >= 56 && v <= 900) gCwAll[i] = (float)v;
    }
    RebuildViews();
}


/* ---------- 通用行绘制（cells 数组 + 截断） ---------- */

static void DrawRowCells(const char (*cells)[220], const Color *colors,
                         const float *cw, int ncols,
                         float x, float rowY, int withIcon)
{
    float cx = x + 8;
    for (int c = 0; c < ncols; c++) {
        float tx = cx;
        float avail = cw[c] - 12;
        if (c == 0 && withIcon) {
            tx += 34;
            avail -= 34;
        }
        DrawTxt(Clip(cells[c], avail), tx, rowY + 11, FS_TXT, colors[c]);
        cx += cw[c];
    }
}

static void DrawRowIcon(const BridgeProc *p, float x, float rowY)
{
    if (!p->path[0]) return;
    Texture2D ic = IconForProc(p->path);
    if (!ic.id) return;
    Rectangle dst = {x + 3, rowY + (ROW_H - 28) / 2, 28, 28};
    Rectangle src = {0, 0, (float)ic.width, (float)ic.height};
    DrawTexturePro(ic, src, dst, (Vector2){0, 0}, 0, WHITE);
}

static int BuildProcCells(const BridgeProc *p, char cells[][220], int indent,
                          int isTree, unsigned long long memOverride, int hasKids)
{
    if (isTree) {
        const char *arrow = hasKids ? (FoldIsOn(p->pid) ? "▸ " : "▾ ") : "";
        if (indent > 0)
            snprintf(cells[0], 220, "%*s%s%s%s", indent * 2, "",
                     indent == 1 ? "└" : "│", arrow, p->name);
        else
            snprintf(cells[0], 220, "%s%s", arrow, p->name);
        snprintf(cells[1], 220, "%lu", (unsigned long)p->pid);
        snprintf(cells[2], 220, "%lu", (unsigned long)p->ppid);
        FormatMem(memOverride ? memOverride : p->memBytes, cells[3], 220);
        snprintf(cells[4], 220, "%s", ProcTypeName(p->type));
        snprintf(cells[5], 220, "%s", p->path[0] ? p->path : P_NOREAD);
        return 6;
    }
    snprintf(cells[0], 220, "%s", p->name);
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
    return isTree ? 6 : 9;
}

static void ProcRowColors(const BridgeProc *p, Color *colors, int ncols)
{
    for (int i = 0; i < ncols; i++)
        colors[i] = gPal.onSurfaceVariant;
    colors[0] = gPal.onSurface;
    if (p->type == 1) colors[0] = gPal.primary;
    else if (p->type == 2) colors[0] = gPal.successC;
    colors[3] = gPal.onSurface;
    colors[4] = p->type ? gPal.primary : gPal.onSurfaceVariant;
}

static void DrawCellText(const char *s, float x, float y, Color c)
{
    /* 截断绘制：scissor 已生效，超宽自然裁剪 */
    DrawTxt(s, x, y, FS_TXT, c);
}

static void DrawProcRowCommon(const BridgeProc *p, float x, float rowY,
                              float *cw, int indent, int isTree,
                              unsigned long long memOverride, int hasKids)
{
    char cells[9][220];
    Color colors[9];
    int ncols = BuildProcCells(p, cells, indent, isTree, memOverride, hasKids);
    ProcRowColors(p, colors, ncols);
    if (ncols >= 9) {
        colors[4] = p->cpuPct > 50.0f ? gPal.errorC
                    : p->cpuPct > 20.0f ? gPal.warnC
                    : gPal.onSurfaceVariant;
    }
    if (!isTree && p->aiRisk >= 3 && ncols >= 9)
        colors[8] = gPal.errorC;
    DrawRowIcon(p, x, rowY);
    DrawRowCells(cells, colors, cw, ncols, x, rowY, 1);
}

/* 返回是否命中右键 */
static int ProcRowInput(Rectangle rowR, const BridgeProc *p)
{
    if (!PtIn(rowR))
        return 0;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        gApp.selectedPid = (int)p->pid;
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        gApp.selectedPid = (int)p->pid;
        OpenContextMenu(0, GetMousePosition(), (int)p->pid, -1);
        return 1;
    }
    return 0;
}

static void DrawViewAll(float x, float y, float w, float h, int nodePyOnly)
{
    (void)nodePyOnly;
    DrawTableHeader(x, y, w, gColsAll, gCwAll, 8, sSortCol, sSortDesc);
    int baseY = BeginList(x, y, w, h, (float)sFilteredCount, &gApp.scrollProc);
    char cells[9][220];
    Color colors[9];
    for (int i = 0; i < sFilteredCount; i++) {
        float rowY = (float)baseY + i * (float)ROW_H;
        BridgeProc *p = &sFiltered[i];
        if (rowY > y + h) break;
        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        Rectangle rowR = {x, rowY, w, ROW_H};
        if (ProcRowInput(rowR, p)) break;
        int ncols = BuildProcCells(p, cells, 0, 0, 0, 0);
        ProcRowColors(p, colors, ncols);
        if (ncols >= 9) {
            colors[4] = p->cpuPct > 50.0f ? gPal.errorC
                        : p->cpuPct > 20.0f ? gPal.warnC
                        : gPal.onSurfaceVariant;
        }
        if (p->aiRisk >= 3 && ncols >= 9)
            colors[8] = gPal.errorC;
        DrawRowIcon(p, x, rowY);
        DrawRowCells(cells, colors, gCwAll, ncols, x, rowY, 1);
    }
    EndList();
}

static void DrawViewTree(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsTree, gCwTree, 6, NULL, NULL);
    int baseY = BeginList(x, y, w, h, (float)gApp.treeCount, &gApp.scrollTree);
    char cells[6][220];
    Color colors[6];
    for (int i = 0; i < gApp.treeCount; i++) {
        float rowY = (float)baseY + i * (float)ROW_H;
        BridgeProc *p = &gApp.treeProcs.items[i];
        if (rowY > y + h) break;
        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        Rectangle rowR = {x, rowY, w, ROW_H};
        Rectangle arrowZone = {x, rowY, 34 + gApp.treeDepth[i] * 18, ROW_H};
        if (PtIn(arrowZone) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            gApp.treeHasKids[i]) {
            FoldToggle(p->pid);
            RebuildTree();
            continue;
        }
        if (ProcRowInput(rowR, p)) break;
        int ncols = BuildProcCells(p, cells, gApp.treeDepth[i], 1, gApp.treeMem[i],
                                   gApp.treeHasKids[i]);
        ProcRowColors(p, colors, ncols);
        DrawRowIcon(p, x, rowY);
        DrawRowCells(cells, colors, gCwTree, ncols, x, rowY, 1);
    }
    EndList();
}

static void DrawViewProject(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsProj, gCwProj, 6, NULL, NULL);
    int baseY = BeginList(x, y, w, h, (float)gApp.projRowCount, &gApp.scrollProj);
    char cells[6][220];
    Color colors[6];
    int row = 0;
    const char *lastGroup = NULL;
    int alt = 0;
    for (int i = 0; i < sFilteredCount; i++) {
        BridgeProc *p = &gApp.projProcs.items[i];
        const char *g = p->project[0] ? p->project : "";
        if (!lastGroup || strcmp(g, lastGroup) != 0) {
            float rowY = (float)baseY + row * (float)ROW_H;
            if (rowY <= y + h) {
                DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.surfaceVariant);
                int cnt = 0;
                for (int j = i; j < sFilteredCount; j++) {
                    const char *g2 = gApp.projProcs.items[j].project[0]
                                         ? gApp.projProcs.items[j].project : "";
                    if (strcmp(g2, g) != 0) break;
                    cnt++;
                }
                char head[300];
                snprintf(head, sizeof(head), "▾ %s（%d）", g[0] ? g : P_NOPROJ, cnt);
                DrawTxt(head, x + 12, rowY + 10, FS_BTN, gPal.primary);
            }
            row++;
            lastGroup = g;
            alt = 0;
        }
        float rowY = (float)baseY + row * (float)ROW_H;
        if (rowY > y + h + (float)ROW_H) break;
        Rectangle rowR = {x, rowY, w, ROW_H};
        if (PtIn(rowR)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                gApp.selectedPid = (int)p->pid;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                gApp.selectedPid = (int)p->pid;
                OpenContextMenu(0, GetMousePosition(), (int)p->pid, -1);
            }
        }
        if (alt % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        alt++;
        snprintf(cells[0], 220, "%s", p->name);
        snprintf(cells[1], 220, "%lu", (unsigned long)p->pid);
        FormatMem(p->memBytes, cells[2], 220);
        snprintf(cells[3], 220, "%s", ProcTypeName(p->type));
        snprintf(cells[4], 220, "%s", p->cmdline[0] ? p->cmdline : "-");
        colors[0] = gPal.onSurface;
        for (int c = 1; c < 5; c++) colors[c] = gPal.onSurfaceVariant;
        colors[3] = p->type ? gPal.primary : gPal.onSurfaceVariant;
        DrawRowCells(cells, colors, (const float *)gCwProj + 1, 5, x + gCwProj[0],
                     rowY, 0);
        row++;
    }
    EndList();
}

static void DrawViewPorts(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsPort, gCwPort, 7, NULL, NULL);
    int baseY = BeginList(x, y, w, h, (float)sPortRowCount, &gApp.scrollPort);
    char cells[7][220];
    Color colors[7];
    for (int i = 0; i < sPortRowCount; i++) {
        float rowY = (float)baseY + i * (float)ROW_H;
        if (rowY > y + h) break;
        PortRow *r = &sPortRows[i];
        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        if (r->isRange)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H,
                          ColorAlpha(gPal.warnC, 0.10f));

        Rectangle rowR = {x, rowY, w, ROW_H};
        if (PtIn(rowR)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                selPortRow = i;
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                selPortRow = i;
                OpenContextMenu(r->isRange ? 2 : 1, GetMousePosition(), -1, i);
            }
        }
        if (selPortRow == i)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.selBg);

        if (r->isRange) {
            snprintf(cells[0], 220, "%lu-%lu", r->port, r->portEnd);
            snprintf(cells[1], 220, "%s", r->tcp ? "TCP" : "UDP");
            snprintf(cells[2], 220, "%s", P_RANGE);
            for (int c = 0; c < 3; c++) colors[c] = gPal.warnC;
            DrawRowCells(cells, colors, gCwPort, 3, x, rowY, 0);
            continue;
        }
        snprintf(cells[0], 220, "%lu", r->port);
        snprintf(cells[1], 220, "%s", r->tcp ? "TCP" : "UDP");
        snprintf(cells[2], 220, "%lu", (unsigned long)r->pid);
        snprintf(cells[3], 220, "%s", r->p ? r->p->name : P_GONE);
        snprintf(cells[4], 220, "%s", r->p ? ProcTypeName(r->p->type) : "-");
        if (r->p)
            FormatMem(r->p->memBytes, cells[5], 220);
        else
            snprintf(cells[5], 220, "-");
        snprintf(cells[6], 220, "%s", r->p && r->p->path[0] ? r->p->path : "-");
        for (int c = 0; c < 7; c++) colors[c] = c == 1 ? gPal.onSurfaceVariant : gPal.onSurface;
        colors[4] = r->p && r->p->type ? gPal.primary : gPal.onSurfaceVariant;
        colors[3] = r->p ? gPal.onSurface : gPal.outline;
        DrawRowCells(cells, colors, gCwPort, 7, x, rowY, 0);
    }
    EndList();
}

void RefreshLogs(void)
{
    bridge_free_logs(&gApp.logs);
    bridge_load_logs(&gApp.logs);
}

static void DrawViewLogs(float x, float y, float w, float h)
{
    DrawTableHeader(x, y, w, gColsLog, gCwLog, 6, NULL, NULL);
    int baseY = BeginList(x, y, w, h, (float)gApp.logs.count, &gApp.scrollLog);
    char cells[6][220];
    Color colors[6];
    for (size_t i = 0; i < gApp.logs.count; i++) {
        float rowY = (float)baseY + (float)i * (float)ROW_H;
        if (rowY > y + h) break;
        BridgeLog *l = &gApp.logs.items[i];
        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        snprintf(cells[0], 220, "%s", l->timeText);
        snprintf(cells[1], 220, "%s", l->source);
        snprintf(cells[2], 220, "%s", l->name);
        snprintf(cells[3], 220, "%lu", (unsigned long)l->pid);
        snprintf(cells[4], 220, "%s", l->ok ? "成功" : "失败");
        snprintf(cells[5], 220, "%s", l->path[0] ? l->path : "-");
        for (int c = 0; c < 6; c++) colors[c] = gPal.onSurface;
        colors[1] = gPal.onSurfaceVariant;
        colors[4] = l->ok ? gPal.successC : gPal.errorC;
        colors[5] = gPal.onSurfaceVariant;
        DrawRowCells(cells, colors, gCwLog, 6, x, rowY, 0);
    }
    EndList();
}

void DrawCurrentView(float x, float y, float w, float h)
{
    switch (gApp.curTab) {
    case TAB_ALL:     DrawViewAll(x, y, w, h, 0); break;
    case TAB_NODEPY:  DrawViewAll(x, y, w, h, 1); break;
    case TAB_TREE:    DrawViewTree(x, y, w, h); break;
    case TAB_PROJECT: DrawViewProject(x, y, w, h); break;
    case TAB_PORTS:   DrawViewPorts(x, y, w, h); break;
    case TAB_LOGS:    DrawViewLogs(x, y, w, h); break;
    case TAB_AI:      break; /* AI 走模态面板 */
    }
}

/* ---------- 右键菜单 ---------- */

static void MenuLog(const char *fmt, ...)
{
    FILE *f = fopen("build/menu_debug.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap); va_end(ap);
    fclose(f);
}

void OpenContextMenu(int kind, Vector2 pos, int pid, int portIdx)
{
    MenuLog("[menu] open kind=%d pid=%d pos=%.0f,%.0f\n", kind, pid, pos.x, pos.y);
    gApp.menuOpen = 1;
    gApp.menuKind = kind;
    gApp.menuPos = pos;
    gApp.menuPid = pid;
    gApp.menuPortIdx = portIdx;
    if (kind == 2 && portIdx >= 0 && portIdx < sPortRowCount) {
        gApp.menuRange[0] = (int)sPortRows[portIdx].port;
        gApp.menuRange[1] = (int)sPortRows[portIdx].portEnd;
    }
}

static int MenuItem(const char *label, Rectangle *mr, float x, float *y, float w)
{
    Rectangle r = {x, *y, w, 44};
    *mr = r;
    int hit = 0;
    if (PtIn(r)) {
        DrawRectangleRec(r, gPal.rowHover);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            hit = 1;
            MenuLog("[menu] item hit: %s\n", label);
        }
    }
    DrawTxt(label, x + 12, *y + 10, FS_BTN, gPal.onSurface);
    *y += 54;
    return hit;
}

void DrawContextMenu(void)
{
    static double sMenuOpenedAt = 0;
    if (!gApp.menuOpen) { sMenuOpenedAt = 0; return; }
    if (sMenuOpenedAt == 0) {
        sMenuOpenedAt = GetTime();
        MenuLog("[menu] first draw at %.2f\n", sMenuOpenedAt);
    }
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    /* 遮罩关闭（打开后短暂保护，避免同帧关闭） */
    if (GetTime() - sMenuOpenedAt > 0.25 &&
        (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))) {
        gApp.menuOpen = 0;
        sMenuOpenedAt = 0;
        return;
    }

    int itemCount = gApp.menuKind == 0 ? 7 : 3;
    float mw = 460;
    float mh = itemCount * 44 + 22;
    float mx = gApp.menuPos.x, my = gApp.menuPos.y;
    if (mx + mw > W - 8) mx = W - 8 - mw;
    if (my + mh > H - 40) my = H - 40 - mh;

    Rectangle panel = {mx, my, mw, mh};
    /* 阴影 + 高对比容器（深色主题下与背景区分） */
    DrawRectangleRounded((Rectangle){mx + 5, my + 6, mw, mh}, 0.05f, 8,
                         (Color){0, 0, 0, 100});
    Color menuBg = gPal.cardBg;
    if (CurrentTheme() == THEME_DARK)
        menuBg = (Color){0x33, 0x30, 0x2E, 255};
    else
        menuBg = (Color){0xFF, 0xFF, 0xFF, 255};
    DrawRectangleRounded(panel, 0.05f, 8, menuBg);
    DrawRectangleRoundedLines(panel, 0.05f, 8, gPal.primary);

    float y = my + 12;
    Rectangle mr;
    if (gApp.menuKind == 0) {
        if (MenuItem(M_KILL, &mr, mx, &y, mw)) {
            if (gApp.selectedPid > 0) {
                bridge_kill_pid((unsigned int)gApp.selectedPid);
                extern void MainUiRefresh(void);
                MainUiRefresh();
                SetFlashMsg(N_KILLED_SEL);
            }
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_SMARTRE, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p && p->path[0]) {
                char path[300], cmd[300];
                snprintf(path, sizeof(path), "%s", p->path);
                snprintf(cmd, sizeof(cmd), "\"%s\" %s", p->path, p->cmdline);
                bridge_kill_pid(p->pid);
                if (SysRelaunch(path, cmd) == 0)
                    SetFlashMsg("已智能重启 %s", p->name);
                else
                    SetFlashMsg("智能重启失败");
                extern void MainUiRefresh(void);
                MainUiRefresh();
            }
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_AI_ANALY, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p && AiAvailable()) {
                /* 该进程监听端口串 */
                char ports[256];
                int n = 0;
                ports[0] = 0;
                for (size_t k = 0; k < gApp.ports.count && n < 200; k++) {
                    if (gApp.ports.items[k].pid == p->pid) {
                        char one[32];
                        snprintf(one, sizeof(one), "%s%lu", n ? "," : "",
                                 (unsigned long)gApp.ports.items[k].port);
                        strcat(ports + n, one);
                        n += (int)strlen(one);
                    }
                }
                gApp.aiMode = 1;
                gApp.modal = 2;
                if (gApp.aiOutput) gApp.aiOutput[0] = 0;
                AiStartAnalyze(p, ports);
            } else if (!AiAvailable()) {
                SetFlashMsg(A_NOKILO);
            }
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_COPY_PATH, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p) SetClipboardText(p->path[0] ? p->path : "");
            SetFlashMsg(N_COPIED);
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_COPY_CMD, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p) SetClipboardText(p->cmdline[0] ? p->cmdline : "");
            SetFlashMsg(N_COPIED);
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_EXPLORER, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p && p->path[0]) SysShowInExplorer(p->path);
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_TERMINAL, &mr, mx, &y, mw)) {
            BridgeProc *p = FindPid((unsigned long)gApp.selectedPid);
            if (p && p->path[0]) {
                char dir[300];
                snprintf(dir, sizeof(dir), "%s", p->path);
                char *slash = strrchr(dir, '\\');
                if (slash) *slash = 0;
                SysOpenTerminal(dir);
            }
            gApp.menuOpen = 0;
            return;
        }
    } else {
        /* 端口/区间菜单 */
        int port = gApp.menuKind == 1
                       ? (int)sPortRows[gApp.menuPortIdx].port
                       : gApp.menuRange[0];
        if (MenuItem(M_ELEVFIX, &mr, mx, &y, mw)) {
            int rc = SysElevatedFix(port);
            SetFlashMsg(rc == 0 ? N_FIX_STARTED
                                : rc == 1 ? N_FIX_CANCEL : "提权启动失败，请改用复制修复命令");
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_COPYFIX, &mr, mx, &y, mw)) {
            char fix[1024];
            SysCopyFixCommand(port, fix, sizeof(fix));
            SetClipboardText(fix);
            SetFlashMsg("已复制修复命令，请在管理员终端粘贴执行");
            gApp.menuOpen = 0;
            return;
        }
        if (MenuItem(M_OPENURL, &mr, mx, &y, mw)) {
            char url[128];
            snprintf(url, sizeof(url), "http://localhost:%d", port);
            SysOpenUrl(url);
            gApp.menuOpen = 0;
            return;
        }
    }
}

/* ---------- 设置弹窗 ---------- */

static int sSetStartMin, sSetAuto, sSetAutoSec, sSetBalloon;
static int sSetOrphan, sSetOrphanMin, sSetOrphanNP, sSetAnomaly, sSetAutoRun, sSetTheme;

void SettingsLoad(void)
{
    sSetStartMin = bridge_config_bool("StartMinimized", 0);
    sSetAuto = bridge_config_bool("AutoRefresh", 1);
    sSetAutoSec = (int)bridge_config_long("AutoRefreshInterval", 10);
    sSetBalloon = bridge_config_bool("BalloonNotify", 1);
    sSetOrphan = bridge_config_bool("OrphanAutoEnable", 0);
    sSetOrphanMin = (int)bridge_config_long("OrphanIntervalMin", 30);
    sSetOrphanNP = bridge_config_bool("OrphanNodePyOnly", 1);
    sSetAnomaly = bridge_config_bool("AnomalyWatch", 0);
    sSetAutoRun = SysIsAutoRun();
    sSetTheme = (int)CurrentTheme();
}

void SettingsSave(void)
{
    bridge_config_set_bool("StartMinimized", sSetStartMin);
    bridge_config_set_bool("AutoRefresh", sSetAuto);
    bridge_config_set_long("AutoRefreshInterval", sSetAutoSec);
    bridge_config_set_bool("BalloonNotify", sSetBalloon);
    bridge_config_set_bool("OrphanAutoEnable", sSetOrphan);
    bridge_config_set_long("OrphanIntervalMin", sSetOrphanMin);
    bridge_config_set_bool("OrphanNodePyOnly", sSetOrphanNP);
    bridge_config_set_bool("AnomalyWatch", sSetAnomaly);
    if (sSetAutoRun != SysIsAutoRun())
        SysSetAutoRun(sSetAutoRun);
    if (sSetTheme != (int)CurrentTheme())
        ToggleTheme();
    gApp.autoRefreshOn = sSetAuto;
    gApp.autoRefreshSec = sSetAutoSec;
    gApp.orphanAutoEnable = sSetOrphan;
    gApp.orphanIntervalMin = sSetOrphanMin;
    gApp.orphanNodePyOnly = sSetOrphanNP;
    gApp.anomalyWatch = sSetAnomaly;
    gApp.startMinimized = sSetStartMin;
    gApp.balloonNotify = sSetBalloon;
}

static int StepControl(const char *label, Rectangle r, int value, int minV, int maxV, int *out)
{
    /* [-] 值 [+] */
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    Vector2 ts = MeasureTxt(buf, FS_BTN);
    Rectangle minus = {r.x, r.y, 40, 40};
    Rectangle plus = {r.x + r.width - 40, r.y, 40, 40};
    Rectangle mid = {minus.x + 46, r.y + 4, r.width - 92, 32};
    int changed = 0;
    Color btn = gPal.primary;
    DrawRectangleRounded(minus, 0.3f, 6, btn);
    DrawRectangleRounded(plus, 0.3f, 6, btn);
    DrawTxt("-", minus.x + 12, minus.y + 4, FS_TITLE - 4, gPal.onPrimary);
    DrawTxt("+", plus.x + 10, plus.y + 4, FS_TITLE - 4, gPal.onPrimary);
    DrawTxt(label, r.x - MeasureTxt(label, FS_BTN).x - 12, r.y + 8, FS_BTN, gPal.onSurface);
    DrawTxt(buf, mid.x + (mid.width - ts.x) / 2, mid.y + 6, FS_BTN, gPal.onSurface);
    if (PtIn(minus) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && value > minV) {
        (*out)--; changed = 1;
    }
    if (PtIn(plus) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && value < maxV) {
        (*out)++; changed = 1;
    }
    return changed;
}

void DrawSettingsModal(void)
{
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    float pw = 860, ph = 720;
    DrawModalPanel(pw, ph);
    float px = (W - pw) / 2, py = (H - ph) / 2;

    DrawTxt(S_TITLE, px + 20, py + 18, FS_TITLE, gPal.onSurface);

    float y = py + 88;
    float x = px + 28;
    float rowW = pw - 56;

    if (DrawCheckBox(S_STARTMIN, (Rectangle){x, y, rowW, 40}, sSetStartMin))
        sSetStartMin = !sSetStartMin;
    y += 54;
    if (DrawCheckBox(S_AUTOREFRESH, (Rectangle){x, y, rowW - 260, 40}, sSetAuto))
        sSetAuto = !sSetAuto;
    StepControl(S_INTERVAL_S, (Rectangle){x + rowW - 240, y, 240, 40}, sSetAutoSec, 3, 3600,
                &sSetAutoSec);
    y += 54;
    if (DrawCheckBox(S_BALLOON, (Rectangle){x, y, rowW, 40}, sSetBalloon))
        sSetBalloon = !sSetBalloon;
    y += 54;
    if (DrawCheckBox(S_ORPHAN_AUTO, (Rectangle){x, y, rowW - 260, 40}, sSetOrphan))
        sSetOrphan = !sSetOrphan;
    StepControl(S_INTERVAL_M, (Rectangle){x + rowW - 240, y, 240, 40}, sSetOrphanMin, 5, 1440,
                &sSetOrphanMin);
    y += 54;
    if (DrawCheckBox(S_ORPHAN_NP, (Rectangle){x, y, rowW, 40}, sSetOrphanNP))
        sSetOrphanNP = !sSetOrphanNP;
    y += 54;
    if (DrawCheckBox(S_ANOMALY, (Rectangle){x, y, rowW, 40}, sSetAnomaly))
        sSetAnomaly = !sSetAnomaly;
    y += 54;
    if (DrawCheckBox(S_AUTORUN, (Rectangle){x, y, rowW, 40}, sSetAutoRun))
        sSetAutoRun = !sSetAutoRun;
    y += 60;

    DrawTxt(S_THEME, x, y + 5, FS_TXT, gPal.onSurface);
    if (DrawRadio(T_THEME_DARK, (Rectangle){x + 110, y, 130, 40}, sSetTheme == 0))
        sSetTheme = 0;
    if (DrawRadio(T_THEME_LIGHT, (Rectangle){x + 250, y, 130, 40}, sSetTheme == 1))
        sSetTheme = 1;
    y += 64;

    if (DrawTextButton(S_OPENLOGDIR, (Rectangle){x, y, 220, 44}, 1)) {
        extern const char *bridge_log_path_utf8(void);
        char logp[400];
        snprintf(logp, sizeof(logp), "%s", bridge_log_path_utf8());
        char *slash = strrchr(logp, '\\');
        if (slash) *slash = 0;
        SysOpenTerminal(logp);
    }
    if (DrawTextButton(S_CANCEL, (Rectangle){px + pw - 252, py + ph - 68, 116, 46}, 1))
        gApp.modal = 0;
    if (DrawTextButton(S_SAVE, (Rectangle){px + pw - 126, py + ph - 68, 106, 46}, 1)) {
        SettingsSave();
        SetFlashMsg(S_SAVED);
        gApp.modal = 0;
    }
}

/* ---------- 孤儿收集 ---------- */

void CollectOrphanPids(unsigned int **pids, int *count)
{
    *pids = NULL;
    *count = 0;
    int n = (int)gApp.procs.count;
    if (!n) return;
    unsigned int *out = (unsigned int *)malloc((size_t)n * sizeof(unsigned int));
    int on = 0;
    for (int i = 0; i < n; i++) {
        BridgeProc *p = &gApp.procs.items[i];
        if (gApp.orphanNodePyOnly && p->type == 0) continue;
        if (p->ppid == 0 || p->ppid == 4) continue;
        if (!FindPid(p->ppid))
            out[on++] = p->pid;
    }
    if (!on) { free(out); return; }
    *pids = out;
    *count = on;
}

/* ---------- 基线快照 / 对比 ---------- */

void BaselineSave(void)
{
    FILE *f = fopen("baseline.txt", "wb");
    if (!f) {
        SetFlashMsg("基线保存失败（无法写入 baseline.txt）");
        return;
    }
    for (size_t i = 0; i < gApp.procs.count; i++) {
        BridgeProc *p = &gApp.procs.items[i];
        fprintf(f, "%lu\t%s\t%d\t%lu\t%s\n",
                (unsigned long)p->pid, p->name, p->type,
                (unsigned long)p->memBytes, p->path);
    }
    fclose(f);
    char msg[96];
    snprintf(msg, sizeof(msg), A_BASE_SAVED, (int)gApp.procs.count);
    SetFlashMsg("%s", msg);
}

void BaselineCompare(void)
{
    FILE *f = fopen("baseline.txt", "rb");
    if (!f) {
        SetFlashMsg(A_BASE_LOST);
        return;
    }

    /* 读基线 */
    unsigned long bpids[4096];
    char bnames[4096][64];
    int bcount = 0;
    char line[600];
    while (fgets(line, sizeof(line), f) && bcount < 4096) {
        unsigned long pid = 0;
        char name[64] = "";
        if (sscanf(line, "%lu\t%63[^\t]", &pid, name) >= 1 && pid > 0) {
            bpids[bcount] = pid;
            snprintf(bnames[bcount], 64, "%s", name);
            bcount++;
        }
    }
    fclose(f);

    /* 差异：基线消失 + 当前新增(node/python) */
    static char out[32768];
    int used = snprintf(out, sizeof(out), "%s\n\n== 基线中已退出的进程 ==\n", A_BASE_TITLE);
    int gone = 0;
    for (int i = 0; i < bcount; i++) {
        if (!FindPid(bpids[i])) {
            used += snprintf(out + used, sizeof(out) - used, "  - %s (PID %lu)\n",
                             bnames[i], bpids[i]);
            gone++;
        }
    }
    if (!gone)
        used += snprintf(out + used, sizeof(out) - used, "  (无)\n");

    used += snprintf(out + used, sizeof(out) - used, "\n== 新增的 Node/Python 进程 ==\n");
    int added = 0;
    for (size_t i = 0; i < gApp.procs.count; i++) {
        BridgeProc *p = &gApp.procs.items[i];
        if (p->type == 0) continue;
        int inBase = 0;
        for (int j = 0; j < bcount; j++)
            if (bpids[j] == p->pid) { inBase = 1; break; }
        if (!inBase) {
            used += snprintf(out + used, sizeof(out) - used, "  + %s (PID %lu, %.0fMB)\n",
                             p->name, (unsigned long)p->pid,
                             (double)p->memBytes / 1048576.0);
            added++;
        }
    }
    if (!added)
        used += snprintf(out + used, sizeof(out) - used, "  (无)\n");

    AiPanelShowText(out);
}