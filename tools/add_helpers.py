# add_helpers.py — 插入行绘制 helpers（无条件）
import io

P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

if 'static void DrawRowCells(' in t:
    print("already present")
else:
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
    anchor = 'static void DrawCellText(const char *s, float x, float y, Color c)'
    t = t.replace(anchor, helpers + anchor, 1)
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("helpers inserted")
