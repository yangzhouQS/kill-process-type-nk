/* main.c — kill-process-type-nk 主入口
 * 纯 raylib 2D 绘制 + data_bridge 隔离 Win32
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "raylib.h"
#include "data_bridge.h"
#include "tray_bridge.h"

/* ==================== MD3 主题 ==================== */

typedef enum { THEME_DARK = 0, THEME_LIGHT = 1 } MdTheme;

typedef struct {
    Color primary, onPrimary;
    Color surface, onSurface;
    Color surfaceVariant, onSurfaceVariant;
    Color outline, error, success;
    Color cardBg, cardBorder;
    Color rowAlt, rowHover;
    Color toolbarBg;
} MdPalette;

static MdPalette g_md;
static MdTheme s_theme = MD_THEME_DARK;

static void md_style_set(MdTheme t)
{
    s_theme = t;
    if (t == MD_THEME_DARK) {
        g_md.primary            = (Color){0x4F, 0x9C, 0xFF, 255};
        g_md.onPrimary          = (Color){0x00, 0x14, 0x2E, 255};
        g_md.surface            = (Color){0x14, 0x12, 0x11, 255};
        g_md.onSurface          = (Color){0xE5, 0xE2, 0xE0, 255};
        g_md.surfaceVariant     = (Color){0x2A, 0x27, 0x26, 255};
        g_md.onSurfaceVariant   = (Color){0xCA, 0xC7, 0xC5, 255};
        g_md.outline            = (Color){0x5C, 0x58, 0x56, 255};
        g_md.error              = (Color){0xFF, 0x8A, 0x80, 255};
        g_md.success            = (Color){0x69, 0xF0, 0xAE, 255};
        g_md.cardBg             = (Color){0x1E, 0x1C, 0x1B, 255};
        g_md.cardBorder         = (Color){0x38, 0x35, 0x33, 255};
        g_md.rowAlt             = (Color){0x24, 0x22, 0x21, 255};
        g_md.rowHover           = (Color){0x2E, 0x2B, 0x2A, 255};
        g_md.toolbarBg          = (Color){0x1A, 0x18, 0x17, 255};
    } else {
        g_md.primary            = (Color){0x1A, 0x6B, 0x3C, 255};
        g_md.onPrimary          = (Color){0xFF, 0xFF, 0xFF, 255};
        g_md.surface            = (Color){0xFD, 0xF8, 0xF3, 255};
        g_md.onSurface          = (Color){0x1C, 0x1B, 0x1A, 255};
        g_md.surfaceVariant     = (Color){0xE8, 0xE3, 0xDD, 255};
        g_md.onSurfaceVariant   = (Color){0x49, 0x45, 0x44, 255};
        g_md.outline            = (Color){0x79, 0x75, 0x74, 255};
        g_md.error              = (Color){0xB3, 0x26, 0x1E, 255};
        g_md.success            = (Color){0x1B, 0x5E, 0x20, 255};
        g_md.cardBg             = (Color){0xFF, 0xFF, 0xFF, 255};
        g_md.cardBorder         = (Color){0xE0, 0xDB, 0xD5, 255};
        g_md.rowAlt             = (Color){0xF5, 0xF0, 0xEA, 255};
        g_md.rowHover           = (Color){0xE8, 0xE3, 0xDD, 255};
        g_md.toolbarBg          = (Color){0xF8, 0xF3, 0xEE, 255};
    }
}

/* ==================== 应用状态 ==================== */

static int current_view = 0;
static BridgeProcList procs;
static BridgePortList ports;
static BridgeLogList logs;
static char filter_buf[256] = { 0 };
static int filter_len = 0;
static int sort_col = 0;
static int sort_desc = 1;
static int selected_pid = -1;
static Font g_font;
static char status_text[128] = "就绪";

static void md_refresh(void)
{
    bridge_free_processes(&procs);
    bridge_free_ports(&ports);
    bridge_scan_processes(&procs);
    bridge_scan_ports(&ports);
    snprintf(status_text, sizeof(status_text),
             "共 %d 进程 · %d 端口", (int)procs.count, (int)ports.count);
}

static void md_refresh_logs(void)
{
    bridge_free_logs(&logs);
    bridge_load_logs(&logs);
}

/* ==================== MD3 绘制辅助 ==================== */

static Font MD_FONT;
static float MD_FS = 14.0f;
static float MD_FS_S = 13.0f;
static float MD_ROW_H = 30.0f;
static float MD_COL_H = 32.0f;

static void DrawMDText(const char *text, float x, float y, float size, Color color)
{
    DrawTextEx(MD_FONT, text, (Vector2){x, y}, size, 1, color);
}

static void DrawMDButton(Rectangle r, const char *text)
{
    Vector2 mouse = GetMousePosition();
    BOOL hover = CheckCollisionPointRec(mouse, r);
    Color bg = hover ? ColorBrightness(g_md.primary, 0.15f) : g_md.primary;
    DrawRectangleRounded(r, 0.5f, 8, bg);
    Vector2 ts = MeasureTextEx(MD_FONT, text, 14, 1);
    DrawTextEx(MD_FONT, text,
        (Vector2){r.x + (r.width - ts.x)/2, r.y + (r.height - ts.y)/2}, 14, 1,
        g_md.onPrimary);
}

static void DrawCard(float x, float y, float w, float h)
{
    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.015f, 8, g_md.cardBg);
    DrawRectangleRoundedLines((Rectangle){x, y, w, h}, 0.015f, 8, g_md.cardBorder);
}

/* ==================== 布局常量 ==================== */

#define PAD            12.0f
#define TOOLBAR_Y      8.0f
#define TOOLBAR_H      52.0f
#define TABBAR_Y       68.0f
#define TABBAR_H       44.0f
#define FILTER_Y       120.0f
#define FILTER_H       40.0f
#define LIST_Y         168.0f

/* ==================== 视图绘制 ==================== */

static void draw_toolbar(void)
{
    float x = 8, y = TOOLBAR_Y;
    float w = (float)GetScreenWidth() - 16;

    DrawRectangleRounded((Rectangle){x, y, w, TOOLBAR_H}, 0.08f, 8, g_md.toolbarBg);

    float bx = x + 16, by = y + 6;

    DrawMDButton((Rectangle){bx, by, 90, 40}, "刷新"); bx += 98;
    DrawMDButton((Rectangle){bx, by, 100, 40}, "杀选中", selected_pid > 0); bx += 108;
    DrawMDButton((Rectangle){bx, by, 120, 40}, "杀全部Node", TRUE); bx += 128;
    DrawMDButton((Rectangle){bx, by, 130, 40}, "杀全部Python", TRUE);
}

static void draw_tabs(void)
{
    float x = 8, y = TABBAR_Y;
    float w = (float)GetScreenWidth() - 16;
    static const char *labels[] = { "全部进程", "Node/Python", "端口占用", "终止日志" };

    DrawRectangle((int)x, (int)y, (int)w, (int)TABBAR_H, g_md.cardBg);

    float tw = w / 4;
    for (int i = 0; i < 4; i++) {
        BOOL sel = (current_view == i);
        if (sel) {
            DrawRectangle((int)(x + i * tw), (int)y, (int)tw, (int)TABBAR_H,
                          g_md.primary);
            DrawRectangle((int)(x + i * tw), (int)(y + TABBAR_H - 3),
                          (int)tw, 3, g_md.primary);
        }
        float fs = 14;
        Vector2 ts = MeasureTextEx(MD_FONT, labels[i], fs, 1);
        Color tc = sel ? g_md.onPrimary : g_md.onSurfaceVariant;
        DrawTextEx(MD_FONT, labels[i],
            (Vector2){x + i * tw + (tw - ts.x) / 2, y + (TABBAR_H - ts.y) / 2},
            fs, 1, tc);
    }
}

static void draw_filter_bar(void)
{
    float x = 8, y = FILTER_Y;
    float w = (float)GetScreenWidth() - 16;

    DrawRectangleRounded((Rectangle){x, y, w, FILTER_H}, 0.15f, 8, g_md.cardBg);

    DrawMDText("筛选:", x + 12, y + 10, 13, g_md.onSurfaceVariant);
    DrawRectangle((int)(x + 60), (int)(y + 4), (int)(w - 80), (int)(FILTER_H - 8),
                  g_md.surfaceVariant);
    DrawTextEx(MD_FONT, filter_buf, (Vector2){x + 68, y + 10}, 14, 1, g_md.onSurface);
}

static void draw_col_header(float x, float y, float w, const char **cols, const float *cw, int ncols)
{
    DrawRectangle((int)x, (int)y, (int)w, (int)MD_COL_H, g_md.surfaceVariant);
    float cx = x + 8;
    for (int i = 0; i < ncols; i++) {
        Color tc = g_md.onSurfaceVariant;
        if (i == sort_col) {
            tc = g_md.primary;
            const char *arrow = sort_desc ? " ▼" : " ▲";
            char buf[64];
            snprintf(buf, sizeof(buf), "%s%s", cols[i], arrow);
            DrawTextEx(MD_FONT, buf, (Vector2){cx, y + (MD_COL_H - 14) / 2}, 13, 1, tc);
        } else {
            DrawTextEx(MD_FONT, cols[i], (Vector2){cx, y + (MD_COL_H - 14) / 2}, 13, 1, tc);
        }
        cx += cw[i];
    }
    DrawRectangle((int)x, (int)(y + MD_COL_H - 1), (int)w, 1, g_md.outline);
}

/* ==================== 视图：全部进程 ==================== */

static void draw_view_procs(float y, float h)
{
    float x = 8, w = (float)GetScreenWidth() - 16;

    DrawCard(x, y, w, h);

    static const char *cols[] = { "进程名", "PID", "父PID", "内存(MB)", "类型", "项目" };
    static const float cw[] = { 240, 90, 90, 110, 90, 300 };
    draw_col_header(x, y + 4, w, cols, cw, 6);

    float rowY = y + 4 + MD_COL_H;

    for (size_t i = 0; i < procs.count; i++) {
        BridgeProc *p = &procs.items[i];
        char pid[16], ppid[16], mem[32];

        snprintf(pid, sizeof(pid), "%lu", (unsigned long)p->pid);
        snprintf(ppid, sizeof(ppid), "%lu", (unsigned long)p->ppid);
        snprintf(mem, sizeof(mem), "%.1f", (double)p->memBytes / 1048576.0);

        BOOL isSel = ((int)p->pid == selected_pid);
        if (isSel) {
            DrawRectangle((int)x, (int)rowY, (int)w, (int)MD_ROW_H,
                          g_md.surfaceVariant);
        } else if (i % 2 == 1) {
            DrawRectangle((int)x, (int)rowY, (int)w, (int)MD_ROW_H, g_md.rowAlt);
        }

        float cx = x + 8, fs = 13;
        Color tc = g_md.onSurface;
        if (p->type == 1) tc = g_md.primary;
        else if (p->type == 2) tc = (Color){0x69, 0xF0, 0xAE, 255};

        DrawTextEx(MD_FONT, p->name, (Vector2){cx, rowY + 7}, fs, 1, tc);
        cx += cw[0];
        DrawTextEx(MD_FONT, pid, (Vector2){cx, rowY + 7}, fs, 1, tc);
        cx += cw[1];
        DrawTextEx(MD_FONT, ppid, (Vector2){cx, rowY + 7}, fs, 1, tc);
        cx += cw[2];
        DrawTextEx(MD_FONT, mem, (Vector2){cx, rowY + 7}, fs, 1, tc);
        cx += cw[3];
        {
            const char *tl = p->type == 1 ? "Node.js" :
                             (p->type == 2 ? "Python" : "—");
            DrawTextEx(MD_FONT, tl, (Vector2){cx, rowY + 7}, fs, 1, tc);
        }
        cx += cw[4];
        DrawTextEx(MD_FONT, p->project[0] ? "…" : "-", (Vector2){cx, rowY + 7}, fs, 1,
                   g_md.onSurfaceVariant);

        rowY += MD_ROW_H;
    }
}

/* ==================== 视图：端口 ==================== */

static void draw_view_ports(float y, float h)
{
    float x = 8, w = (float)GetScreenWidth() - 16;

    DrawCard(x, y, w, h);

    static const char *cols[] = { "端口", "协议", "PID", "类型" };
    static const float cw[] = { 120, 120, 120, 200 };
    draw_col_header(x, y + 4, w, cols, cw, 4);

    float rowY = y + 4 + MD_COL_H;

    for (size_t i = 0; i < ports.count; i++) {
        BridgePort *p = &ports.items[i];
        char port[16], pid[16];

        snprintf(port, sizeof(port), "%lu", (unsigned long)p->port);
        snprintf(pid, sizeof(pid), "%lu", (unsigned long)p->pid);

        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, (int)MD_ROW_H, g_md.rowAlt);

        float cx = x + 8, fs = 13;
        DrawTextEx(MD_FONT, port, (Vector2){cx, rowY + 6}, fs, 1, g_md.onSurface);
        cx += cw[0];
        DrawTextEx(MD_FONT, p->tcp ? "TCP" : "UDP", (Vector2){cx, rowY + 6}, fs, 1,
                   g_md.onSurfaceVariant);
        cx += cw[1];
        DrawTextEx(MD_FONT, pid, (Vector2){cx, rowY + 6}, fs, 1, g_md.onSurface);
        cx += cw[2];
        DrawTextEx(MD_FONT, p->ipv6 ? "IPv6" : "IPv4", (Vector2){cx, rowY + 6}, fs, 1,
                   g_md.onSurfaceVariant);

        rowY += MD_ROW_H;
    }
}

/* ==================== 视图：日志 ==================== */

static void draw_view_logs(float y, float h)
{
    float x = 8, w = (float)GetScreenWidth() - 16;

    DrawCard(x, y, w, h);

    static const char *cols[] = { "时间", "来源", "进程名", "PID", "结果", "路径" };
    static const float cw[] = { 160, 100, 160, 80, 80, 300 };
    draw_col_header(x, y + 4, w, cols, cw, 6);

    float rowY = y + 4 + MD_COL_H;

    for (size_t i = 0; i < logs.count; i++) {
        BridgeLog *l = &logs.items[i];
        char pid[16];

        snprintf(pid, sizeof(pid), "%lu", (unsigned long)l->pid);

        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, (int)MD_ROW_H, g_md.rowAlt);

        float cx = x + 8, fs = 13;
        Color rc = l->ok ? g_md.success : g_md.error;
        DrawTextEx(MD_FONT, l->timeText, (Vector2){cx, rowY + 6}, fs, 1, g_md.onSurface);
        cx += cw[0];
        DrawTextEx(MD_FONT, l->source, (Vector2){cx, rowY + 6}, fs, 1,
                   g_md.onSurfaceVariant);
        cx += cw[1];
        DrawTextEx(MD_FONT, l->name, (Vector2){cx, rowY + 6}, fs, 1, g_md.onSurface);
        cx += cw[2];
        DrawTextEx(MD_FONT, pid, (Vector2){cx, rowY + 6}, fs, 1, g_md.onSurface);
        cx += cw[3];
        DrawTextEx(MD_FONT, l->ok ? "已终止" : "失败", (Vector2){cx, rowY + 6}, fs, 1, rc);
        cx += cw[4];
        DrawTextEx(MD_FONT, l->path[0] ? l->path : "-", (Vector2){cx, rowY + 6}, fs, 1,
                   g_md.onSurfaceVariant);

        rowY += MD_ROW_H;
    }
}

/* ==================== 主入口 ==================== */

int main(void)
{
    /* 初始化 */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1200, 700, "kill-process-type-nk");
    SetTargetFPS(60);

    /* 加载字体（尝试加载系统 YaHei，失败用默认） */
    {
        const char *fontPaths[] = {
            "C:\\Windows\\Fonts\\msyh.ttc",
            "C:\\Windows\\Fonts\\simhei.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
        };
        BOOL loaded = FALSE;
        for (int i = 0; i < 3 && !loaded; i++) {
            if (FileExists(fontPaths[i])) {
                g_font = LoadFontEx(fontPaths[i], 28, NULL, 250);
                SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
                loaded = TRUE;
            }
        }
        if (!loaded) {
            g_font = GetFontDefault();
            loaded = TRUE;
        }
        MD_FONT = g_font;
    }

    /* 主题 */
    md_style_set(MD_THEME_DARK);

    /* 初始扫描 */
    memset(&procs, 0, sizeof(procs));
    memset(&ports, 0, sizeof(ports));
    memset(&logs, 0, sizeof(logs));
    md_refresh();

    /* 托盘 */
    tray_init();

    /* 主循环 */
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(g_md.surface);

        draw_toolbar();
        draw_tabs();
        draw_filter_bar();

        float listY = LIST_Y;
        float listH = (float)GetScreenHeight() - listY - PAD - 32;

        if (current_view <= 1)
            draw_view_procs(listY, listH);
        else if (current_view == 2)
            draw_view_ports(listY, listH);
        else if (current_view == 3)
            draw_view_logs(listY, listH);

        /* 状态栏 */
        {
            float sy = (float)GetScreenHeight() - 28;
            DrawRectangle(0, (int)sy, GetScreenWidth(), 28, g_md.toolbarBg);
            DrawTextEx(MD_FONT, status_text, (Vector2){PAD, sy + 7}, 13, 1,
                       g_md.onSurfaceVariant);
        }

        EndDrawing();
    }

    /* 清理 */
    tray_shutdown();
    bridge_free_processes(&procs);
    bridge_free_ports(&ports);
    bridge_free_logs(&logs);
    CloseWindow();
    return 0;
}
