# patch12.py — 选中高亮增强/首行裁切/modal输入门禁
import io

# ============ app_shared.h: UiInputBlocked 声明 ============
P = r"src\app_shared.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "UiInputBlocked" not in t:
    t = t.replace("const char *Clip(const char *s, float maxW); /* 超宽截断（…结尾） */",
                  "const char *Clip(const char *s, float maxW); /* 超宽截断（…结尾） */\nint UiInputBlocked(void); /* modal/菜单打开时底层交互禁用 */")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("hdr ok")

# ============ app_shared.c: selBg 加深 + UiInputBlocked 实现 + 表头/列表门禁 ============
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

# 1) selBg 加深（两主题）
rep("gPal.selBg            = (Color){0x2C,0x3E,0x55,255};",
    "gPal.selBg            = (Color){0x3A,0x55,0x78,255};", "selbg-dark")
rep("gPal.selBg            = (Color){0xD3,0xE8,0xD8,255};",
    "gPal.selBg            = (Color){0xB7,0xD7F,0xFF,255};" if False else "gPal.selBg            = (Color){0xBE,0xD9,0xFB,255};", "selbg-light")

# 2) UiInputBlocked 实现（SetFlashMsg 前）
rep("void SetFlashMsg(const char *fmt, ...)",
    """int UiInputBlocked(void)
{
    return gApp.modal != 0 || gApp.menuOpen;
}

void SetFlashMsg(const char *fmt, ...)""", "blocked-impl")

# 3) DrawTableHeader：排序点击与列宽拖拽加门禁
rep("""        static int dragCol = -1;
        static float dragX = 0, dragW = 0;
        Vector2 m = GetMousePosition();
        if (dragCol >= 0) {""",
"""        static int dragCol = -1;
        static float dragX = 0, dragW = 0;
        Vector2 m = GetMousePosition();
        if (UiInputBlocked()) {
            dragCol = -1;
        } else if (dragCol >= 0) {""", "hdr-drag-gate")
rep("""        if (PointInRect(hr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && sortCol
            && !PointInRect((Rectangle){cx + cw[i] - 16, y, 16, 44})) {""",
"""        if (PointInRect(hr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && sortCol
            && !UiInputBlocked()
            && !PointInRect((Rectangle){cx + cw[i] - 16, y, 16, 44})) {""", "hdr-sort-gate")

# 4) BeginList 滚轮门禁 + scissor 上沿 1px 余量
rep("""    Rectangle listR = {x, y, w, h};
    if (PointInRect(listR)) {
        float mw = GetMouseWheelMove();
        if (mw != 0) {
            *scroll -= mw * 60.0f;
            if (*scroll < 0) *scroll = 0;
            if (*scroll > maxOff) *scroll = maxOff;
        }
    }
    BeginScissorMode((int)x, (int)(y + 44), (int)w, (int)viewH);""",
"""    Rectangle listR = {x, y, w, h};
    if (PointInRect(listR) && !UiInputBlocked()) {
        float mw = GetMouseWheelMove();
        if (mw != 0) {
            *scroll -= mw * 60.0f;
            if (*scroll < 0) *scroll = 0;
            if (*scroll > maxOff) *scroll = maxOff;
        }
    }
    BeginScissorMode((int)x, (int)(y + 43), (int)w, (int)viewH + 1);""", "beginlist")

# 5) DrawTabBar 页签点击门禁
rep("""            if (PointInRect(tr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                gApp.curTab = i;""",
"""            if (PointInRect(tr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !UiInputBlocked()) {
                gApp.curTab = i;""", "tab-gate")

# 6) DrawToolbar 按钮门禁
rep("""        if (DrawTextButton(btns[i].label, (Rectangle){bx, by, btns[i].w, 46}, 1)) {""",
"""        if (!UiInputBlocked() && DrawTextButton(btns[i].label, (Rectangle){bx, by, btns[i].w, 46}, 1)) {""", "toolbar-gate")
rep("""    if (DrawTextButton(themeLabel, (Rectangle){rx, by, tw, 46}, 1))""",
    """    if (!UiInputBlocked() && DrawTextButton(themeLabel, (Rectangle){rx, by, tw, 46}, 1))""", "toolbar-theme-gate")
rep("""    if (DrawTextButton(T_SETTINGS, (Rectangle){rx, by, 108, 46}, 1)) {""",
    """    if (!UiInputBlocked() && DrawTextButton(T_SETTINGS, (Rectangle){rx, by, 108, 46}, 1)) {""", "toolbar-settings-gate")
rep("""    if (DrawTextButton(T_AI_CHAT, (Rectangle){rx, by, 142, 46}, 1)) {""",
    """    if (!UiInputBlocked() && DrawTextButton(T_AI_CHAT, (Rectangle){rx, by, 142, 46}, 1)) {""", "toolbar-ai-gate")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("app_shared MISS:", miss if miss else "none")

# ============ ui_views.c: ProcRowInput 门禁 + 文本垂直微调 + 选中左边条 ============
P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss2 = []

def rep2(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss2.append(tag)

# ProcRowInput 门禁
rep2("""static int ProcRowInput(Rectangle rowR, const BridgeProc *p)
{
    if (!PtIn(rowR))
        return 0;""",
"""static int ProcRowInput(Rectangle rowR, const BridgeProc *p)
{
    if (UiInputBlocked())
        return 0;
    if (!PtIn(rowR))
        return 0;""", "row-gate")

# DrawRowCells 文本垂直微调 11 -> 12（字形顶部余量）
rep2("""        DrawTxt(Clip(cells[c], avail), tx, rowY + 11, FS_TXT, colors[c]);""",
    """        DrawTxt(Clip(cells[c], avail), tx, rowY + 12, FS_TXT, colors[c]);""", "cell-y")

# 选中行主色左边条（DrawProcRowCommon/DirectRow 绘制处——在 sel 背景绘制后加）
# DrawViewAll/Tree/ProcRowCommon 均走 DrawProcRowCommon 或直绘——统一在两处 sel 矩形绘制后加条
rep2("""    if (sel)
        DrawRectangle((int)x, (int)rowY, (int)(GetScreenWidth() - 16), ROW_H, gPal.selBg);""",
    """    if (sel) {
        DrawRectangle((int)x, (int)rowY, (int)(GetScreenWidth() - 16), ROW_H, gPal.selBg);
        DrawRectangle((int)x, (int)rowY, 3, ROW_H, gPal.primary);
    }""", "sel-bar-common")

# DrawViewProject 的选中行（有自己的绘制）
rep2("""        if (alt % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        alt++;
        snprintf(cells[0], 220, "%s", p->name);""",
    """        if (alt % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.rowAlt);
        if ((int)p->pid == gApp.selectedPid)
            DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.selBg);
        alt++;
        snprintf(cells[0], 220, "%s", p->name);""", "proj-sel")

# 端口选中行已有 selBg ✓

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("ui_views MISS:", miss2 if miss2 else "none")

# ============ main.c: modal 时跳过底层组件与右键 ============
P = r"src\main.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss3 = []

def rep3(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss3.append(tag)

rep3("""        DrawToolbar();
        DrawTabBar();
        DrawFilterBar();

        float listY = 214;
        float listH = (float)GetScreenHeight() - listY - 36;
        DrawCurrentView(8, listY, (float)GetScreenWidth() - 16, listH);

        DrawStatusBar();
        DrawContextMenu();""",
"""        DrawToolbar();
        DrawTabBar();
        DrawFilterBar();

        float listY = 214;
        float listH = (float)GetScreenHeight() - listY - 36;
        if (gApp.modal == 0)
            DrawCurrentView(8, listY, (float)GetScreenWidth() - 16, listH);

        DrawStatusBar();
        if (gApp.modal == 0)
            DrawContextMenu();""", "modal-gate")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("main MISS:", miss3 if miss3 else "none")
