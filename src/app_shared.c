/* app_shared.c — 主题/字体/全局状态/通用控件 实现 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "raylib.h"
#include "app_shared.h"
#include "ui_text.h"
#include "font_codepoints.h"
#include "ai_bridge.h"
#include "ui_views.h"
#include "tray_bridge.h"

/* ================= 全局状态 ================= */
AppState gApp;
Palette gPal;
Font gFont;
static AppTheme sTheme = THEME_DARK;
static double sFlashUntil = 0;
static char sFlashBuf[256] = "";

void SetFlashMsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sFlashBuf, sizeof(sFlashBuf), fmt, ap);
    va_end(ap);
    sFlashUntil = GetTime() + 8.0;
}

AppTheme CurrentTheme(void) { return sTheme; }

void ApplyTheme(AppTheme t)
{
    sTheme = t;
    gApp.theme = t;
    if (t == THEME_DARK) {
        gPal.primary          = (Color){0x4F,0x9C,0xFF,255};
        gPal.onPrimary        = (Color){0x00,0x14,0x2E,255};
        gPal.primaryContainer = (Color){0x1E,0x3A,0x5F,255};
        gPal.surface          = (Color){0x14,0x12,0x11,255};
        gPal.surfaceVariant   = (Color){0x2A,0x27,0x26,255};
        gPal.onSurface        = (Color){0xE5,0xE2,0xE0,255};
        gPal.onSurfaceVariant = (Color){0xCA,0xC7,0xC5,255};
        gPal.outline          = (Color){0x5C,0x58,0x56,255};
        gPal.errorC           = (Color){0xFF,0x8A,0x80,255};
        gPal.successC         = (Color){0x69,0xF0,0xAE,255};
        gPal.warnC            = (Color){0xFF,0xD5,0x4F,255};
        gPal.cardBg           = (Color){0x1E,0x1C,0x1B,255};
        gPal.cardBorder       = (Color){0x38,0x35,0x33,255};
        gPal.rowAlt           = (Color){0x24,0x22,0x21,255};
        gPal.rowHover         = (Color){0x2E,0x2B,0x2A,255};
        gPal.toolbarBg        = (Color){0x1A,0x18,0x17,255};
        gPal.selBg            = (Color){0x2C,0x3E,0x55,255};
    } else {
        gPal.primary          = (Color){0x1A,0x6B,0x3C,255};
        gPal.onPrimary        = (Color){0xFF,0xFF,0xFF,255};
        gPal.primaryContainer = (Color){0xC8,0xEF,0xD5,255};
        gPal.surface          = (Color){0xFD,0xF8,0xF3,255};
        gPal.surfaceVariant   = (Color){0xE8,0xE3,0xDD,255};
        gPal.onSurface        = (Color){0x1C,0x1B,0x1A,255};
        gPal.onSurfaceVariant = (Color){0x49,0x45,0x44,255};
        gPal.outline          = (Color){0x79,0x75,0x74,255};
        gPal.errorC           = (Color){0xB3,0x26,0x1E,255};
        gPal.successC         = (Color){0x1B,0x5E,0x20,255};
        gPal.warnC            = (Color){0xB2,0x6A,0x00,255};
        gPal.cardBg           = (Color){0xFF,0xFF,0xFF,255};
        gPal.cardBorder       = (Color){0xE0,0xDB,0xD5,255};
        gPal.rowAlt           = (Color){0xF5,0xF0,0xEA,255};
        gPal.rowHover         = (Color){0xE8,0xE3,0xDD,255};
        gPal.toolbarBg        = (Color){0xF8,0xF3,0xEE,255};
        gPal.selBg            = (Color){0xD3,0xE8,0xD8,255};
    }
}

void ToggleTheme(void)
{
    ApplyTheme(sTheme == THEME_DARK ? THEME_LIGHT : THEME_DARK);
    bridge_config_set_long("ui.theme", (long)sTheme);
}

/* ================= 字体 ================= */
void LoadAppFont(void)
{
    const char *paths[] = {
        "assets/fonts/msyh.ttf",
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
    };
    for (int i = 0; i < 4; i++) {
        if (!FileExists(paths[i]))
            continue;
        Font f = LoadFontEx(paths[i], 28, kFontCodepoints, FONT_CODEPOINT_COUNT);
        if (f.baseSize == 28 && f.glyphCount > 0) {
            gFont = f;
            SetTextureFilter(gFont.texture, TEXTURE_FILTER_BILINEAR);
            printf("[UI] font ok: %s (%d glyphs)\n", paths[i], f.glyphCount);
            return;
        }
    }
    gFont = GetFontDefault();
    printf("[UI] all fonts failed, fallback to default\n");
}

void DrawTxt(const char *text, float x, float y, float size, Color c)
{
    DrawTextEx(gFont, text, (Vector2){x, y}, size, 1, c);
}

const char *Clip(const char *s, float maxW)
{
    static char bufs[8][1024];
    static int idx = 0;
    char *b = bufs[idx];
    idx = (idx + 1) & 7;
    if (MeasureTxt(s, FS_TXT).x <= maxW)
        return s;
    snprintf(b, 1024, "%s", s);
    float target = maxW - MeasureTxt("…", FS_TXT).x - 2;
    int len = (int)strlen(b);
    while (len > 0 && MeasureTxt(b, FS_TXT).x > target) {
        len--;
        while (len > 0 && (b[len] & 0xC0) == 0x80) len--;
        b[len] = 0;
    }
    strcat(b, "…");
    return b;
}

Vector2 MeasureTxt(const char *text, float size)
{
    return MeasureTextEx(gFont, text, size, 1);
}

void FormatMem(unsigned long long bytes, char *buf, int cap)
{
    snprintf(buf, cap, "%.1f MB", (double)bytes / 1048576.0);
}

const char *ProcTypeName(int type)
{
    if (type == 1) return TY_NODE;
    if (type == 2) return TY_PY;
    return TY_OTHER;
}

/* ================= 进程图标缓存 ================= */

#define ICON_CACHE_MAX 160

typedef struct {
    char path[260];
    Texture2D tex;
} IconEntry;

static IconEntry sIconCache[ICON_CACHE_MAX];
static int sIconCacheCount = 0;
static Texture2D sIconFallback;
static int sIconFallbackInit = 0;

Texture2D IconForProc(const char *path)
{
    /* 命中 */
    for (int i = 0; i < sIconCacheCount; i++) {
        if (strcmp(sIconCache[i].path, path) == 0)
            return sIconCache[i].tex;
    }
    if (sIconCacheCount >= ICON_CACHE_MAX)
        return sIconFallback;

    /* 提取 */
    Texture2D tex = {0};
    if (path[0]) {
        int w = 0, h = 0;
        unsigned char *px = SysExtractIconRGBA(path, 32, &w, &h);
        if (px && w > 0) {
            Image img = {
                .data = px,
                .width = w,
                .height = h,
                .mipmaps = 1,
                .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
            };
            tex = LoadTextureFromImage(img);
            free(px);
        }
    }
    if (tex.id == 0) {
        /* 兜底：一次性生成 1 像素透明纹理 */
        if (!sIconFallbackInit) {
            Image img = GenImageColor(1, 1, BLANK);
            sIconFallback = LoadTextureFromImage(img);
            UnloadImage(img);
            sIconFallbackInit = 1;
        }
        return sIconFallback;
    }

    snprintf(sIconCache[sIconCacheCount].path, 260, "%s", path);
    sIconCache[sIconCacheCount].tex = tex;
    sIconCacheCount++;
    return tex;
}

void IconCacheFree(void)
{
    for (int i = 0; i < sIconCacheCount; i++)
        UnloadTexture(sIconCache[i].tex);
    sIconCacheCount = 0;
    if (sIconFallbackInit) {
        UnloadTexture(sIconFallback);
        sIconFallbackInit = 0;
    }
}

/* ================= UTF-8 换行工具 ================= */

static const char *Utf8Next(const char *s)
{
    int adv = 0;
    GetCodepoint(s, &adv);
    if (adv <= 0) adv = 1;
    return s + adv;
}

int WrapCount(const char *text, float maxW, float size)
{
    int lines = 1;
    float lw = 0;
    const char *p = text;
    while (*p) {
        if (*p == '\n') { lines++; lw = 0; p++; continue; }
        const char *q = Utf8Next(p);
        char tmp[8];
        int n = (int)(q - p);
        if (n > 7) n = 7;
        memcpy(tmp, p, n);
        tmp[n] = 0;
        float gw = MeasureTxt(tmp, size).x;
        lw += gw;
        if (lw > maxW) { lines++; lw = gw; }
        p = q;
    }
    return lines;
}

void DrawWrapped(const char *text, float x, float y, float maxW, float size,
                 Color c, float *scroll)
{
    (void)scroll;
    float cy = y;
    const char *seg = text;
    const char *p = text;
    while (1) {
        if (*p == 0 || *p == '\n') {
            const char *q = seg;
            float rowW = 0;
            while (q < p) {
                const char *nxt = Utf8Next(q);
                char tmp[8];
                int n = (int)(nxt - q);
                if (n > 7) n = 7;
                memcpy(tmp, q, n);
                tmp[n] = 0;
                float gw = MeasureTxt(tmp, size).x;
                if (rowW + gw > maxW && q > seg) {
                    DrawTextEx(gFont, seg, (Vector2){x, cy}, size, 1, c);
                    /* 只画到 q 前：用 scissor 太重，改为逐行绘制片段 */
                    cy += size + 4;
                    seg = q;
                    rowW = gw;
                } else {
                    rowW += gw;
                }
                q = nxt;
            }
            if (p > seg) {
                /* 片段绘制：宽字节临时截断 */
                char buf[2048];
                int n = (int)(p - seg);
                if (n > 2047) n = 2047;
                memcpy(buf, seg, n);
                buf[n] = 0;
                DrawTextEx(gFont, buf, (Vector2){x, cy}, size, 1, c);
            }
            if (*p == 0) break;
            cy += size + 4;
            p++;
            seg = p;
        } else {
            p++;
        }
    }
}

/* ================= 通用控件 ================= */

static int PointInRect(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

int DrawTextButton(const char *label, Rectangle r, int enabled)
{
    int clicked = 0;
    int hover = enabled && PointInRect(r);
    Color bg = enabled ? gPal.primary : ColorAlpha(gPal.primary, 0.35f);
    if (hover) bg = ColorBrightness(bg, 0.15f);
    DrawRectangleRounded(r, 0.35f, 8, bg);
    Vector2 ts = MeasureTxt(label, FS_BTN);
    DrawTxt(label, r.x + (r.width - ts.x) / 2, r.y + (r.height - ts.y) / 2 + 1,
            FS_BTN, enabled ? gPal.onPrimary : gPal.onSurfaceVariant);
    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        clicked = 1;
    return clicked;
}

int DrawCheckBox(const char *label, Rectangle r, int checked)
{
    int clicked = 0;
    Rectangle box = {r.x, r.y + (r.height - 20) / 2, 20, 20};
    Rectangle lblR = {r.x + 28, r.y, r.width - 28, r.height};
    int hover = PointInRect(box) || PointInRect(lblR);
    if (hover) {
        if (PointInRect(lblR))
            DrawRectangleRec(lblR, gPal.rowHover);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            clicked = 1;
    }
    DrawRectangleRounded(box, 0.25f, 4, checked ? gPal.primary : (Color){0,0,0,0});
    DrawRectangleRoundedLines(box, 0.25f, 4, gPal.outline);
    if (checked) {
        float cx = box.x + box.width / 2, cy = box.y + box.height / 2;
        DrawLineEx((Vector2){cx - 5, cy}, (Vector2){cx - 1, cy + 4}, 2.5f, gPal.onPrimary);
        DrawLineEx((Vector2){cx - 1, cy + 4}, (Vector2){cx + 5, cy - 4}, 2.5f, gPal.onPrimary);
    }
    DrawTxt(label, lblR.x + 4, r.y + (r.height - 19) / 2, FS_TXT, gPal.onSurface);
    return clicked;
}

int DrawRadio(const char *label, Rectangle r, int selected)
{
    int clicked = 0;
    Rectangle cir = {r.x, r.y + (r.height - 20) / 2, 20, 20};
    if ((PointInRect(cir) || PointInRect(r)) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        clicked = 1;
    DrawCircleLines((int)(cir.x + 10), (int)(cir.y + 10), 9.5f, gPal.outline);
    if (selected)
        DrawCircle((int)(cir.x + 10), (int)(cir.y + 10), 5.5f, gPal.primary);
    DrawTxt(label, cir.x + 28, r.y + (r.height - 19) / 2, FS_TXT, gPal.onSurface);
    return clicked;
}

void DrawModalPanel(float w, float h)
{
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    DrawRectangleRec((Rectangle){0, 0, W, H}, (Color){0, 0, 0, 120});
    Rectangle panel = {(W - w) / 2, (H - h) / 2, w, h};
    DrawRectangleRounded(panel, 0.03f, 10, gPal.cardBg);
    DrawRectangleRoundedLines(panel, 0.03f, 10, gPal.outline);
}

/* ================= 工具栏 ================= */

void DrawToolbar(void)
{
    float W = (float)GetScreenWidth();
    DrawRectangleRounded((Rectangle){8, 8, W - 16, 72}, 0.10f, 8, gPal.toolbarBg);

    float bx = 20, by = 19;
    struct { const char *label; float w; int id; } btns[] = {
        {T_REFRESH, 108, 0}, {T_KILL_SEL, 142, 1},
        {T_KILL_NODE, 178, 2}, {T_KILL_PY, 190, 3},
        {T_CLEAN_ORPHAN, 142, 4}, {T_ADMIN, 142, 5},
    };
    for (int i = 0; i < 5; i++) {
        if (DrawTextButton(btns[i].label, (Rectangle){bx, by, btns[i].w, 46}, 1)) {
            extern void MainToolbarAction(int id);
            MainToolbarAction(btns[i].id);
        }
        bx += btns[i].w + 8;
    }

    float rx = W - 20;
    const char *themeLabel = (sTheme == THEME_DARK) ? T_THEME_LIGHT : T_THEME_DARK;
    float tw = MeasureTxt(themeLabel, FS_BTN).x + 36;
    rx -= tw;
    if (DrawTextButton(themeLabel, (Rectangle){rx, by, tw, 46}, 1))
        ToggleTheme();
    rx -= 8;
    rx -= 108;
    if (DrawTextButton(T_SETTINGS, (Rectangle){rx, by, 108, 46}, 1)) {
        gApp.modal = 1;
        SettingsLoad();
    }
    rx -= 8;
    rx -= 142;
    if (DrawTextButton(T_AI_CHAT, (Rectangle){rx, by, 142, 46}, 1)) {
        gApp.modal = 2;
        gApp.aiMode = 0;
    }
}

/* ================= 页签 ================= */

void DrawTabBar(void)
{
    static const char *labels[TAB_COUNT] = {
        TT_TAB_ALL, TT_TAB_NODEPY, TT_TAB_TREE, TT_TAB_PROJECT, TT_TAB_PORTS, TT_TAB_LOGS, TT_TAB_AI
    };
    float x = 8, y = 88;
    float W = (float)GetScreenWidth() - 16;

    DrawRectangle((int)x, (int)y, (int)W, 40, gPal.cardBg);
    {
        static const char *labels[TAB_COUNT] = {
            TT_TAB_ALL, TT_TAB_NODEPY, TT_TAB_TREE, TT_TAB_PROJECT,
            TT_TAB_PORTS, TT_TAB_LOGS, TT_TAB_AI
        };
        float cx = x;
        for (int i = 0; i < TAB_COUNT; i++) {
            float tw = MeasureTxt(labels[i], FS_BTN).x + 48;
            Rectangle tr = {cx, y + 4, tw, 32};
            int sel = (gApp.curTab == i);
            if (PointInRect(tr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                gApp.curTab = i;
                if (i == TAB_LOGS) {
                    extern void RefreshLogs(void);
                    RefreshLogs();
                }
            }
            if (sel)
                DrawRectangleRec(tr, gPal.primary);
            else if (PointInRect(tr))
                DrawRectangleRec(tr, gPal.rowHover);
            Vector2 ts = MeasureTxt(labels[i], FS_BTN);
            DrawTxt(labels[i], tr.x + (tw - ts.x) / 2, tr.y + (32 - ts.y) / 2 + 1,
                    FS_BTN, sel ? gPal.onPrimary : gPal.onSurfaceVariant);
            cx += tw + 4;
        }
    }
}

/* ================= 筛选栏 ================= */

void DrawFilterBar(void)
{
    float x = 8, y = 152;
    float W = (float)GetScreenWidth() - 16;
    DrawRectangleRounded((Rectangle){x, y, W, 50}, 0.20f, 8, gPal.cardBg);
    DrawTxt("筛选", x + 12, y + 13, FS_BTN, gPal.onSurfaceVariant);

    Rectangle editR = {x + 86, y + 6, W - 100, 38};
    DrawRectangleRounded(editR, 0.20f, 6, gPal.surfaceVariant);

    DrawTxt(gApp.filterBuf, editR.x + 8, editR.y + 7, FS_TXT,
            gApp.filterLen ? gPal.onSurface : gPal.onSurfaceVariant);
    if (!gApp.filterLen)
        DrawTxt(T_FILTER_HINT, editR.x + 8, editR.y + 5, 15, gPal.outline);
    if (((int)(GetTime() * 2.0)) % 2 == 0) {
        float cx = editR.x + 8 + MeasureTxt(gApp.filterBuf, 15).x;
        DrawRectangleRec((Rectangle){cx, editR.y + 4, 1.5f, 20}, gPal.primary);
    }
}

/* ================= 状态栏 ================= */

void DrawStatusBar(void)
{
    float sy = (float)GetScreenHeight() - 38;
    float W = (float)GetScreenWidth();
    DrawRectangle(0, (int)sy, (int)W, 38, gPal.toolbarBg);
    if (GetTime() < sFlashUntil)
        DrawTxt(sFlashBuf, 12, sy + 9, FS_HDR, gPal.warnC);
    else
        DrawTxt(gApp.statusText, 12, sy + 9, FS_HDR, gPal.onSurfaceVariant);
}

/* ================= 表头/列表 ================= */

void DrawTableHeader(float x, float y, float w, const char **cols,
                     const float *cw, int ncols, int *sortCol,
                     int *sortDesc)
{
    DrawRectangle((int)x, (int)y, (int)w, 44, gPal.surfaceVariant);

    /* 列宽拖拽：靠近列边界 ±8px 按住左右拖动 */
    {
        static int dragCol = -1;
        static float dragX = 0, dragW = 0;
        Vector2 m = GetMousePosition();
        if (dragCol >= 0) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float nw = dragW + (m.x - dragX);
                if (nw < 56) nw = 56;
                ((float *)cw)[dragCol] = nw;
                SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
            } else {
                dragCol = -1;
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);
                UiOnColumnResize();
            }
        } else if (PointInRect((Rectangle){x, y, w, 44})) {
            float edge = x + 8;
            for (int c = 0; c < ncols - 1; c++) {
                edge += cw[c];
                if (m.x > edge - 8 && m.x < edge + 8) {
                    SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        dragCol = c;
                        dragX = m.x;
                        dragW = cw[c];
                    }
                    break;
                }
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            }
        }
    }

    float cx = x + 8;
    for (int i = 0; i < ncols; i++) {
        Rectangle hr = {cx - 8, y, cw[i], 44};
        if (sortCol && i == *sortCol) {
            /* 排序列：文字 + 几何三角（不依赖字形） */
            DrawTxt(cols[i], cx, y + 12, FS_HDR, gPal.primary);
            Vector2 t1, t2, t3;
            if (*sortDesc) {
                t1 = (Vector2){cx + MeasureTxt(cols[i], FS_HDR).x + 8, y + 18};
                t2 = (Vector2){cx + MeasureTxt(cols[i], FS_HDR).x + 24, y + 18};
                t3 = (Vector2){cx + MeasureTxt(cols[i], FS_HDR).x + 16, y + 28};
            } else {
                t1 = (Vector2){cx + MeasureTxt(cols[i], FS_HDR).x + 8, y + 28};
                t2 = (Vector2){cx + MeasureTxt(cols[i], FS_HDR).x + 24, y + 28};
                t3 = (Vector2){cx + MeasureTxt(cols[i], FS_HDR).x + 16, y + 18};
            }
            DrawTriangle(t1, t2, t3, gPal.primary);
        } else {
            DrawTxt(Clip(cols[i], cw[i] - 26), cx, y + 12, FS_HDR,
                    gPal.onSurfaceVariant);
        }
        if (PointInRect(hr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && sortCol
            && !PointInRect((Rectangle){cx + cw[i] - 16, y, 16, 44})) {
            if (*sortCol == i) *sortDesc = !*sortDesc;
            else { *sortCol = i; *sortDesc = 0; }
        }
        cx += cw[i];
    }
    DrawRectangle((int)x, (int)(y + 43), (int)w, 1, gPal.outline);
}

int BeginList(float x, float y, float w, float h, float contentRows, float *scroll)
{
    float viewH = h - 44;
    float contentH = contentRows * (float)ROW_H;
    float maxOff = contentH > viewH ? contentH - viewH : 0.0f;
    Rectangle listR = {x, y, w, h};
    if (PointInRect(listR)) {
        float mw = GetMouseWheelMove();
        if (mw != 0) {
            *scroll -= mw * 60.0f;
            if (*scroll < 0) *scroll = 0;
            if (*scroll > maxOff) *scroll = maxOff;
        }
    }
    BeginScissorMode((int)x, (int)(y + 44), (int)w, (int)viewH);
    return (int)(y + 30 - *scroll);
}

void EndList(void)
{
    EndScissorMode();
}


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
        const char *nl = strchr(p, '\n');
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
                         x + 10, cy, maxW - 10, size, gPal.onSurfaceVariant);
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

/* ================= AI 面板 ================= */

static void AiAppendOut(const char *text)
{
    int addLen = (int)strlen(text);
    if (gApp.aiOutputCap == 0) {
        gApp.aiOutputCap = 8192;
        gApp.aiOutput = (char *)calloc(1, (size_t)gApp.aiOutputCap);
    }
    int used = (int)strlen(gApp.aiOutput);
    if (used + addLen + 1 > gApp.aiOutputCap) {
        while (used + addLen + 1 > gApp.aiOutputCap)
            gApp.aiOutputCap *= 2;
        gApp.aiOutput = (char *)realloc(gApp.aiOutput, (size_t)gApp.aiOutputCap);
    }
    memcpy(gApp.aiOutput + used, text, (size_t)addLen + 1);
}

void AiPanelShowText(const char *text)
{
    if (gApp.aiOutput)
        gApp.aiOutput[0] = 0;
    if (text) {
        /* 借用 AiAppendOut 内部逻辑：先确保容量 */
        if (gApp.aiOutputCap == 0) {
            gApp.aiOutputCap = 8192;
            gApp.aiOutput = (char *)calloc(1, (size_t)gApp.aiOutputCap);
        }
        int addLen = (int)strlen(text);
        if (addLen + 1 > gApp.aiOutputCap) {
            gApp.aiOutputCap = addLen + 64;
            gApp.aiOutput = (char *)realloc(gApp.aiOutput, (size_t)gApp.aiOutputCap);
        }
        memcpy(gApp.aiOutput, text, (size_t)addLen + 1);
    }
    gApp.aiNeedResetScroll = 1;
    gApp.modal = 2;
    gApp.aiMode = 1;
}

void AiApplyRisk(void)
{
    if (!gApp.aiOutput)
        return;
    unsigned int pids[256];
    int risks[256];
    int n = AiParseRiskJson(gApp.aiOutput, pids, risks, 256);
    int applied = 0;
    for (int i = 0; i < n; i++) {
        for (size_t k = 0; k < gApp.procs.count; k++) {
            if (gApp.procs.items[k].pid == pids[i]) {
                gApp.procs.items[k].aiRisk = risks[i];
                applied++;
                break;
            }
        }
    }
    char msg[96];
    snprintf(msg, sizeof(msg), A_SCAN_DONE, applied);
    SetFlashMsg("%s", msg);
    if (applied > 0)
        RebuildViews();
}

void AiApplyCleanStrategy(void)
{
    if (!gApp.aiOutput)
        return;
    unsigned int pids[256];
    int n = AiParseCleanJson(gApp.aiOutput, pids, 256);
    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (bridge_kill_pid(pids[i]) == 0)
            ok++;
    }
    if (n > 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), A_APPLY_DONE, ok);
        SetFlashMsg("%s", msg);
        if (gApp.balloonNotify)
            tray_notify("AI 清理策略", msg);
        RebuildViews();
    }
}

void DrawAiJobPoll(void)
{
    int st = AiPoll();
    if (st == 2) {
        const char *res = AiGetResult();
        if (gApp.aiMode == 2) {
            if (gApp.aiOutput) gApp.aiOutput[0] = 0;
            AiAppendOut(res);
            AiApplyRisk();
        } else if (gApp.aiMode >= 1) {
            if (gApp.aiOutput) gApp.aiOutput[0] = 0;
            AiAppendOut(res);
        } else {
            AiAppendOut("\n----\n");
            AiAppendOut(res);
        }
        gApp.aiNeedResetScroll = 1;
        gApp.aiRunning = 0;
        AiConsumeResult();
    } else if (st == 3) {
        const char *err = AiGetResult();
        AiAppendOut("\n[失败] ");
        AiAppendOut(err && err[0] ? err : A_FAIL);
        gApp.aiRunning = 0;
        AiConsumeResult();
    } else if (st == 1) {
        gApp.aiRunning = 1;
    }
}

void AiChatSubmit(void)
{
    if (gApp.aiInputLen && !gApp.aiRunning) {
        AiAppendOut("\n[我] ");
        AiAppendOut(gApp.aiInput);
        AiStartChat(gApp.aiInput);
        gApp.aiInputLen = 0;
        gApp.aiInput[0] = 0;
    }
}

void DrawAiPanel(void)
{
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    float pw = 720, ph = H - 160;
    DrawModalPanel(pw, ph);
    float px = (W - pw) / 2, py = (H - ph) / 2;

    DrawTxt(A_TITLE, px + 20, py + 14, FS_TITLE, gPal.onSurface);
    /* 功能按钮行 */
    {
        struct { const char *label; int w; int mode; } fbtns[] = {
            {A_SCAN, 100, 2}, {A_REVIEW, 100, 3},
            {A_DIAG, 100, 4}, {A_STRATEGY, 100, 5},
        };
        float fbx = px + 190;
        for (int i = 0; i < 4; i++) {
            if (DrawTextButton(fbtns[i].label,
                               (Rectangle){fbx, py + 12, fbtns[i].w, 38}, !gApp.aiRunning)) {
                gApp.aiMode = fbtns[i].mode;
                if (gApp.aiOutput) gApp.aiOutput[0] = 0;
                if (fbtns[i].mode == 2) AiStartRiskScan(&gApp.procs);
                else if (fbtns[i].mode == 3) AiStartLogReview(&gApp.logs);
                else if (fbtns[i].mode == 4) AiStartDiag(&gApp.procs);
                else AiStartCleanStrategy(&gApp.procs);
            }
            fbx += fbtns[i].w + 6;
        }
    }
    if (DrawTextButton(A_CLOSE, (Rectangle){px + pw - 104, py + 12, 84, 38}, 1)) {
        gApp.modal = 0;
        return;
    }

    Rectangle outR = {px + 16, py + 60, pw - 32, ph - 148};
    DrawRectangleRounded(outR, 0.03f, 6, gPal.surfaceVariant);

    float contentRows = 4;
    if (gApp.aiOutput && gApp.aiOutput[0])
        contentRows = (float)WrapCount(gApp.aiOutput, outR.width - 16, FS_TXT) + 2;

    if (gApp.aiNeedResetScroll) {
        gApp.scrollAiOut = 1e9f;
        gApp.aiNeedResetScroll = 0;
    }
    int baseY = BeginList(outR.x, outR.y, outR.width, outR.height, contentRows,
                          &gApp.scrollAiOut);
    /* BeginList 的 maxOff 用行数近似，重置滚动后本帧 clamp 自然生效 */
    if (gApp.aiOutput && gApp.aiOutput[0])
        DrawRich(gApp.aiOutput, outR.x + 8, (float)baseY + 4,
                 outR.width - 16, FS_TXT, gPal.onSurface, &gApp.scrollAiOut);
    else
        DrawTxt("输入问题，或右键进程选择 AI 分析…", outR.x + 8, (float)baseY + 8,
                15, gPal.outline);
    if (gApp.aiRunning)
        DrawTxt(A_RUNNING, outR.x + outR.width - 130, (float)baseY + 4, FS_HDR, gPal.warnC);
    EndList();

    if (gApp.aiMode == 5 && !gApp.aiRunning && gApp.aiOutput && gApp.aiOutput[0]) {
        if (DrawTextButton(A_APPLY, (Rectangle){px + 16, py + ph - 70, 150, 40}, 1)) {
            AiApplyCleanStrategy();
            gApp.aiMode = 1;
        }
    }

    if (gApp.aiMode == 0) {
        Rectangle inR = {px + 16, py + ph - 74, pw - 32 - 96, 40};
        DrawRectangleRounded(inR, 0.15f, 6, gPal.surfaceVariant);
        DrawTxt(gApp.aiInputLen ? gApp.aiInput : A_CHAT_PH, inR.x + 8, inR.y + 8, FS_TXT,
                gApp.aiInputLen ? gPal.onSurface : gPal.outline);
        float caretX = inR.x + 8 + MeasureTxt(gApp.aiInput, FS_TXT).x;
        if (((int)(GetTime() * 2.0)) % 2 == 0)
            DrawRectangleRec((Rectangle){caretX, inR.y + 6, 1.5f, 22}, gPal.primary);

        if (DrawTextButton(A_SEND, (Rectangle){inR.x + inR.width + 8, inR.y, 88, 40}, 1)
            && gApp.aiInputLen && !gApp.aiRunning) {
            AiAppendOut("\n[我] ");
            AiAppendOut(gApp.aiInput);
            AiStartChat(gApp.aiInput);
            gApp.aiInputLen = 0;
            gApp.aiInput[0] = 0;
        }
    }
}
