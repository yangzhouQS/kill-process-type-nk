# fix_views.py — 视图行绘制重构为 cells 数组 + 统一 Clip 截断
import io
import re

P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

# ---------- helper 代码块 ----------
helpers = r'''/* ---------- 通用行绘制（cells 数组 + 截断） ---------- */

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
                          int isTree, unsigned long long memOverride)
{
    if (isTree) {
        if (indent > 0)
            snprintf(cells[0], 220, "%*s%s %s", indent * 2, "",
                     indent == 1 ? "└" : "│", p->name);
        else
            snprintf(cells[0], 220, "%s", p->name);
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
    snprintf(cells[4], 220, "%s", ProcTypeName(p->type));
    snprintf(cells[5], 220, "%s", p->path[0] ? p->path : P_NOREAD);
    snprintf(cells[6], 220, "%s", p->cmdline[0] ? p->cmdline : "-");
    snprintf(cells[7], 220, "%s",
             p->aiRisk ? (p->aiRisk >= 3 ? "高" : p->aiRisk == 2 ? "中" : "低") : "-");
    return isTree ? 6 : 8;
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

'''

# ---------- 替换 DrawProcRowCommon ----------
new_common = r'''static void DrawProcRowCommon(const BridgeProc *p, float x, float rowY,
                              float *cw, int indent, int isTree,
                              unsigned long long memOverride)
{
    char cells[8][220];
    Color colors[8];
    int ncols = BuildProcCells(p, cells, indent, isTree, memOverride);
    ProcRowColors(p, colors, ncols);
    if (!isTree && p->aiRisk >= 3 && ncols >= 8)
        colors[7] = gPal.errorC;
    DrawRowIcon(p, x, rowY);
    DrawRowCells(cells, colors, cw, ncols, x, rowY, 1);
}
'''

m = re.search(r'static void DrawProcRowCommon\(.*?\n\}\n', t, re.S)
if not m:
    print("MISS DrawProcRowCommon")
else:
    t = t[:m.start()] + new_common + t[m.end():]

# ---------- 替换 DrawViewAll ----------
new_all = r'''static void DrawViewAll(float x, float y, float w, float h, int nodePyOnly)
{
    (void)nodePyOnly;
    DrawTableHeader(x, y, w, gColsAll, gCwAll, 8, sSortCol, sSortDesc);
    int baseY = BeginList(x, y, w, h, (float)sFilteredCount, &gApp.scrollProc);
    char cells[8][220];
    Color colors[8];
    for (int i = 0; i < sFilteredCount; i++) {
        float rowY = (float)baseY + i * (float)ROW_H;
        BridgeProc *p = &sFiltered[i];
        if (rowY > y + h) break;
        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        Rectangle rowR = {x, rowY, w, ROW_H};
        if (ProcRowInput(rowR, p)) break;
        int ncols = BuildProcCells(p, cells, 0, 0, 0);
        ProcRowColors(p, colors, ncols);
        if (p->aiRisk >= 3 && ncols >= 8)
            colors[7] = gPal.errorC;
        DrawRowIcon(p, x, rowY);
        DrawRowCells(cells, colors, gCwAll, ncols, x, rowY, 1);
    }
    EndList();
}
'''
m = re.search(r'static void DrawViewAll\(.*?\n\}\n', t, re.S)
if not m:
    print("MISS DrawViewAll")
else:
    t = t[:m.start()] + new_all + t[m.end():]

# ---------- 替换 DrawViewTree ----------
new_tree = r'''static void DrawViewTree(float x, float y, float w, float h)
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
        if (ProcRowInput(rowR, p)) break;
        int ncols = BuildProcCells(p, cells, gApp.treeDepth[i], 1, gApp.treeMem[i]);
        ProcRowColors(p, colors, ncols);
        DrawRowIcon(p, x, rowY);
        DrawRowCells(cells, colors, gCwTree, ncols, x, rowY, 1);
    }
    EndList();
}
'''
m = re.search(r'static void DrawViewTree\(.*?\n\}\n', t, re.S)
if not m:
    print("MISS DrawViewTree")
else:
    t = t[:m.start()] + new_tree + t[m.end():]

# ---------- 替换 DrawViewProject ----------
new_proj = r'''static void DrawViewProject(float x, float y, float w, float h)
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
'''
m = re.search(r'static void DrawViewProject\(.*?\n\}\n', t, re.S)
if not m:
    print("MISS DrawViewProject")
else:
    t = t[:m.start()] + new_proj + t[m.end():]

# ---------- 替换 DrawViewPorts ----------
new_ports = r'''static void DrawViewPorts(float x, float y, float w, float h)
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
'''
m = re.search(r'static void DrawViewPorts\(.*?\n\}\n', t, re.S)
if not m:
    print("MISS DrawViewPorts")
else:
    t = t[:m.start()] + new_ports + t[m.end():]

# ---------- 替换 DrawViewLogs ----------
new_logs = r'''static void DrawViewLogs(float x, float y, float w, float h)
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
'''
m = re.search(r'static void DrawViewLogs\(.*?\n\}\n', t, re.S)
if not m:
    print("MISS DrawViewLogs")
else:
    t = t[:m.start()] + new_logs + t[m.end():]

# ---------- 清理：旧 DrawCellText/rowY+9 遗留引用检查 ----------
with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("views rewritten")
