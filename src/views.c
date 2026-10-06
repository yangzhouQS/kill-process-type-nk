/* main.c - kill-process-type-nk
 * Pure raylib 2D UI, Material Design 3 style.
 * Data access through data_bridge (isolates Win32 from raylib).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "raylib.h"
#include "data_bridge.h"
#include "tray_bridge.h"

/* ============ Theme ============ */

typedef enum { THEME_DARK = 0, THEME_LIGHT = 1 } AppTheme;

typedef struct {
    Color primary, onPrimary;
    Color surface, onSurface;
    Color surfaceVariant, onSurfaceVariant;
    Color outline;
    Color error, success;
    Color cardBg, cardBorder;
    Color rowAlt, rowHover;
    Color toolbarBg;
} Palette;

static Palette pal;
static Theme cur_theme = THEME_DARK;

static void apply_theme(Theme t)
{
    cur_theme = t;
    if (t == THEME_DARK) {
        pal.primary            = (Color){0x4F,0x9C,0xFF,255};
        pal.onPrimary          = (Color){0x00,0x14,0x2E,255};
        pal.surface            = (Color){0x14,0x12,0x11,255};
        pal.onSurface          = (Color){0xE5,0xE2,0xE0,255};
        pal.surfaceVariant     = (Color){0x2A,0x27,0x26,255};
        pal.onSurfaceVariant   = (Color){0xCA,0xC7,0xC5,255};
        pal.outline            = (Color){0x5C,0x58,0x56,255};
        pal.error              = (Color){0xFF,0x8A,0x80,255};
        pal.success            = (Color){0x69,0xF0,0xAE,255};
        pal.cardBg             = (Color){0x1E,0x1C,0x1B,255};
        pal.cardBorder         = (Color){0x38,0x35,0x33,255};
        pal.rowAlt             = (Color){0x24,0x22,0x21,255};
        pal.rowHover           = (Color){0x2E,0x2B,0x2A,255};
        pal.toolbarBg          = (Color){0x1A,0x18,0x17,255};
    } else {
        pal.primary            = (Color){0x1A,0x6B,0x3C,255};
        pal.onPrimary          = (Color){0xFF,0xFF,0xFF,255};
        pal.surface            = (Color){0xFD,0xF8,0xF3,255};
        pal.onSurface          = (Color){0x1C,0x1B,0x1A,255};
        pal.surfaceVariant     = (Color){0xE8,0xE3,0xDD,255};
        pal.onSurfaceVariant   = (Color){0x49,0x45,0x44,255};
        pal.outline            = (Color){0x79,0x75,0x74,255};
        pal.error              = (Color){0xB3,0x26,0x1E,255};
        pal.success            = (Color){0x1B,0x5E,0x20,255};
        pal.cardBg             = (Color){0xFF,0xFF,0xFF,255};
        pal.cardBorder         = (Color){0xE0,0xDB,0xD5,255};
        pal.rowAlt             = (Color){0xF5,0xF0,0xEA,255};
        pal.rowHover           = (Color){0xE8,0xE3,0xDD,255};
        pal.toolbarBg          = (Color){0xF8,0xF3,0xEE,255};
    }
}

/* ============ State ============ */

static int cur_tab = 0;
static BridgeProcList procs;
static BridgePortList ports;
static BridgeLogList logs;
static char filter_buf[256];
static int filter_len;
static int selected_pid = -1;
static Font app_font;
static char status_text[128] = "Ready";

static void refresh_data(void)
{
    bridge_free_processes(&procs);
    bridge_free_ports(&ports);
    bridge_scan_processes(&procs);
    bridge_scan_ports(&ports);
    snprintf(status_text, sizeof(status_text),
             "Procs: %d  Ports: %d", (int)procs.count, (int)ports.count);
}

static void refresh_logs(void)
{
    bridge_free_logs(&logs);
    bridge_load_logs(&logs);
}

/* ============ Font ============ */

static Font app_font;

static void load_font(void)
{
    const char *paths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
    };
    for (int i = 0; i < 3; i++) {
        if (FileExists(paths[i])) {
            app_font = LoadFontEx(paths[i], 28, NULL, 250);
            SetTextureFilter(app_font.texture, TEXTURE_FILTER_BILINEAR);
            return;
        }
    }
    app_font = GetFontDefault();
}

#define TXT(x, y, txt, sz, c) DrawTextEx(app_font, txt, (Vector2){(x), (y)}, sz, 1, c)

/* ============ UI drawing helpers ============ */

static void DrawTextC(const char *text, float x, float y, float sz, Color c)
{
    DrawTextEx(app_font, text, (Vector2){x, y}, sz, 1, c);
}

static Vector2 MeasureTextC(const char *text, float sz)
{
    return MeasureTextEx(app_font, text, sz, 1);
}

static void DrawRoundedBtn(Rectangle r, const char *text, int isHover)
{
    Color bg = isHover ? ColorBrightness(pal.primary, 0.15f) : pal.primary;
    DrawRectangleRounded(r, 0.4f, 8, bg);
    Vector2 ts = MeasureTextC(text, 13);
    DrawTextC(text, r.x + (r.width - ts.x)/2, r.y + (r.height - ts.y)/2, 13, pal.onPrimary);
}

static void DrawCard(Rectangle r)
{
    DrawRectangleRounded(r, 0.015f, 8, pal.cardBg);
    DrawRectangleRoundedLines(r, 0.015f, 8, pal.cardBorder);
}

static void DrawToolbar(void)
{
    float W = (float)GetScreenWidth();
    DrawRectangleRounded((Rectangle){8, 8, W - 16, 52}, 0.08f, 8, pal.toolbarBg);

    float bx = 20, by = 14;
    const char *btnLabels[] = {"Refresh", "Kill Sel", "Kill Node", "Kill Py"};
    float btnW[] = {90, 100, 140, 130};
    for (int i = 0; i < 4; i++) {
        DrawRoundedBtn((Rectangle){bx, by, btnW[i], 28}, btnLabels[i], 0);
        bx += btnW[i] + 8;
    }
}

static void DrawTabs(void)
{
    float x = 8, y = 68;
    float W = (float)GetScreenWidth() - 16;
    static const char *labels[] = {"All Processes", "Node/Python", "Ports", "Logs"};

    DrawRectangle((int)x, (int)y, (int)W, (int)44, pal.cardBg);

    float tw = (W - 16) / 4;
    for (int i = 0; i < 4; i++) {
        int sel = (cur_tab == i);
        if (sel)
            DrawRectangle((int)(x + i * tw), (int)y, (int)tw, (int)44, pal.primary);
        DrawTextEx(app_font, labels[i],
            (Vector2){x + i * tw + tw / 2 - MeasureTextC(labels[i], 14).x / 2,
                      y + 14}, 14, 1,
            sel ? pal.onPrimary : pal.onSurfaceVariant);
    }

    if (cur_tab >= 0 && cur_tab < 4)
        DrawRectangle((int)(x + cur_tab * tw), (int)(y + 44 - 3), (int)tw, 3, pal.primary);
}

static void DrawFilterBar(void)
{
    float x = 8, y = 120;
    float W = (float)GetScreenWidth() - 16;
    DrawRectangleRounded((Rectangle){x, y, W - 16, 40}, 0.15f, 8, pal.cardBg);
    DrawTextC("Filter:", x + 12, y + 10, 13, pal.onSurfaceVariant);
    DrawRectangle((int)(x + 60), (int)(y + 4), (int)(W - 90), (int)(32),
                  pal.surfaceVariant);
}

static void DrawStatusBar(void)
{
    float sy = (float)GetScreenHeight() - 28;
    DrawRectangle(0, (int)sy, GetScreenWidth(), 28, pal.toolbarBg);
    DrawTextEx(app_font, status_text, (Vector2){12, sy + 7}, 13, 1,
               pal.onSurfaceVariant);
}

/* ============ Column header ============ */

static void DrawColHeader(float x, float y, float w,
                          const char **cols, const float *cw, int ncols)
{
    DrawRectangle((int)x, (int)y, (int)w, (int)32, pal.surfaceVariant);
    float cx = x + 8;
    for (int i = 0; i < ncols; i++) {
        Color tc = pal.onSurfaceVariant;
        if (i == sort_col) {
            tc = pal.primary;
            const char *arrow = sort_desc ? " v" : " ^";
            char buf[64];
            snprintf(buf, sizeof(buf), "%s%s", cols[i], arrow);
            DrawTextEx(app_font, buf, (Vector2){cx, y + (32 - 14) / 2}, 13, 1, tc);
        } else {
            DrawTextEx(app_font, cols[i], (Vector2){cx, y + (32 - 14) / 2}, 13, 1, tc);
        }
        cx += cw[i];
    }
    DrawRectangle((int)x, (int)(y + 32 - 1), (int)w, 1, pal.outline);
}

/* ============ Process list (ALL tab) ============ */

static const char *proc_cols[] = {"Name", "PID", "PPID", "Mem(MB)", "Type", "Project"};
static const float proc_cw[] = {240, 90, 90, 110, 90, 300};

static void DrawProcList(float x, float y, float w, float h)
{
    DrawCard(x, y, w, h);
    DrawColHeader(x, y + 4, w, proc_cols, proc_cw, 6);

    float rowY = y + 4 + 32;

    for (size_t i = 0; i < procs.count; i++) {
        BridgeProc *p = &procs.items[i];
        char pid[16], ppid[16], mem[32];

        snprintf(pid, sizeof(pid), "%lu", (unsigned long)p->pid);
        snprintf(ppid, sizeof(ppid), "%lu", (unsigned long)p->ppid);
        snprintf(mem, sizeof(mem), "%.1f", (double)p->memBytes / 1048576.0);

        BOOL isSel = ((int)p->pid == selected_pid);
        if (isSel) {
            DrawRectangle((int)x, (int)rowY, (int)w, (int)30,
                          pal.surfaceVariant);
        } else if (i % 2 == 1) {
            DrawRectangle((int)x, (int)rowY, (int)w, (int)30, pal.rowAlt);
        }

        float cx = x + 8, fs = 13;
        Color tc = pal.onSurface;
        if (p->type == 1) tc = pal.primary;
        else if (p->type == 2) tc = (Color){0x69, 0xF0, 0xAE, 255};

        DrawTextEx(app_font, p->name, (Vector2){cx, rowY + 7}, fs, 1, tc);
        cx += proc_cw[0];
        DrawTextEx(app_font, pid, (Vector2){cx, rowY + 7}, fs, 1, tc);
        cx += proc_cw[1];
        DrawTextEx(app_font, ppid, (Vector2){cx, rowY + 7}, fs, 1, tc);
        cx += proc_cw[2];
        DrawTextEx(app_font, mem, (Vector2){cx, rowY + 7}, fs, 1, tc);
        cx += proc_cw[3];
        {
            const char *tl = p->type == 1 ? "Node.js" :
                             (p->type == 2 ? "Python" : "-");
            DrawTextEx(app_font, tl, (Vector2){cx, rowY + 7}, fs, 1, tc);
        }
        cx += proc_cw[4];
        DrawTextEx(app_font, p->project[0] ? "..." : "-", (Vector2){cx, rowY + 7}, fs, 1,
                   pal.onSurfaceVariant);

        rowY += 30;
    }
}

/* ============ Port list (PORTS tab) ============ */

static void DrawPortList(float x, float y, float w, float h)
{
    DrawCard(x, y, w, h);
    DrawColHeader(x, y + 4, w, port_cols, port_cw, 6);

    float rowY = y + 4 + 32;

    for (size_t i = 0; i < ports.count; i++) {
        BridgePort *p = &ports.items[i];
        char port[16], pid[16];

        snprintf(port, sizeof(port), "%lu", (unsigned long)p->port);
        snprintf(pid, sizeof(pid), "%lu", (unsigned long)p->pid);

        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, (int)30, pal.rowAlt);

        float cx = x + 8, fs = 13;
        DrawTextEx(app_font, port, (Vector2){cx, rowY + 6}, fs, 1, pal.onSurface);
        cx += port_cw[0];
        DrawTextEx(app_font, p->proto, (Vector2){cx, rowY + 6}, fs, 1,
                   pal.onSurfaceVariant);
        cx += port_cw[1];
        DrawTextEx(app_font, pid, (Vector2){cx, rowY + 6}, fs, 1, pal.onSurface);
        cx += port_cw[2];
        DrawTextEx(app_font, p->type, (Vector2){cx, rowY + 6}, fs, 1,
                   pal.onSurfaceVariant);

        rowY += 30;
    }
}

/* ============ Log list (LOGS tab) ============ */

static const char *log_cols[] = {"Time", "Source", "Name", "PID", "Result", "Path"};
static const float log_cw[] = {160, 100, 160, 80, 80, 300};

static void DrawLogList(float x, float y, float w, float h)
{
    DrawCard(x, y, w, h);
    DrawColHeader(x, y + 4, w, log_cols, log_cw, 6);

    float rowY = y + 4 + 32;

    for (size_t i = 0; i < logs.count; i++) {
        BridgeLog *l = &logs.items[i];
        char pid[16];

        snprintf(pid, sizeof(pid), "%lu", (unsigned long)l->pid);

        if (i % 2 == 1)
            DrawRectangle((int)x, (int)rowY, (int)w, (int)30, pal.rowAlt);

        float cx = x + 8, fs = 13;
        Color rc = l->ok ? pal.success : pal.error;
        DrawTextEx(app_font, l->timeText, (Vector2){cx, rowY + 6}, fs, 1, pal.onSurface);
        cx += log_cw[0];
        DrawTextEx(app_font, l->source, (Vector2){cx, rowY + 6}, fs, 1,
                   pal.onSurfaceVariant);
        cx += log_cw[1];
        DrawTextEx(app_font, l->name, (Vector2){cx, rowY + 6}, fs, 1, pal.onSurface);
        cx += log_cw[2];
        DrawTextEx(app_font, pid, (Vector2){cx, rowY + 6}, fs, 1, pal.onSurface);
        cx += log_cw[3];
        DrawTextEx(app_font, l->ok ? "OK" : "FAIL", (Vector2){cx, rowY + 6}, fs, 1, rc);
        cx += log_cw[4];
        DrawTextEx(app_font, l->path[0] ? l->path : "-", (Vector2){cx, rowY + 6}, fs, 1,
                   pal.onSurfaceVariant);

        rowY += 30;
    }
}
