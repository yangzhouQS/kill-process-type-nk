/* main.c — kill-process-type-nk 主入口
 * 纯 raylib 2D 绘制（MD3 风格），通过 data_bridge 隔离 Win32
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "raylib.h"
#include "data_bridge.h"
#include "tray_bridge.h"

/* ==================== 主题 ==================== */

typedef enum { THEME_DARK = 0, THEME_LIGHT = 1 } Theme;

typedef struct {
    Color primary, onPrimary;
    Color surface, onSurface;
    Color surfaceVariant, onSurfaceVariant;
    Color outline, error, success;
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

/* ==================== 状态 ==================== */

static int cur_tab = 0;
static BridgeProcList procs;
static BridgePortList ports;
static BridgeLogList logs;
static char filter_buf[256];
static int filter_len;
static int selected_pid = -1;
static Font g_font;
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

/* ==================== Font ==================== */

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

#define F(x, y, txt, sz, c) DrawTextEx(app_font, txt, (Vector2){(x), (y)}, sz, 1, c)

/* ==================== Main ==================== */

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1200, 700, "kill-process-type-nk");
    SetTargetFPS(60);

    load_font();
    apply_theme(THEME_DARK);

    memset(&procs, 0, sizeof(procs));
    memset(&ports, 0, sizeof(ports));
    memset(&logs, 0, sizeof(logs));
    refresh_data();

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(pal.surface);

        /* Toolbar */
        DrawRectangleRounded((Rectangle){8, 8, (float)GetScreenWidth() - 16, 52},
                             0.08f, 8, pal.toolbarBg);
        float bx = 20, by = 14;
        const char *btnLabels[] = {"Refresh", "Kill Sel", "Kill Node", "Kill Py"};
        float btnW[] = {90, 100, 140, 130};
        for (int i = 0; i < 4; i++) {
            DrawRectangleRounded((Rectangle){bx, by, btnW[i], 28}, 0.4f, 8, pal.primary);
            DrawTextEx(app_font, btnLabels[i],
                (Vector2){bx + 8, by + 7}, 13, 1, pal.onPrimary);
            bx += btnW[i] + 8;
        }

        /* Tab bar */
        DrawRectangle(8, 68, (int)(W - 16), 44, pal.cardBg);
        for (int i = 0; i < 4; i++) {
            int sel = (cur_tab == i);
            if (sel)
                DrawRectangle((int)(8 + i * 110), (int)(68), (int)110, (int)44,
                              pal.primary);
            DrawTextEx(app_font, tabs[i],
                (Vector2){8 + i * 110 + 8, 68 + 14}, 14, 1,
                sel ? pal.onPrimary : pal.onSurfaceVariant);
        }

        /* Filter bar */
        DrawRectangleRounded((Rectangle){8, 120, (float)GetScreenWidth() - 16, 40},
                             0.15f, 8, pal.cardBg);
        DrawTextEx(app_font, "Filter:", (Vector2){20, 120 + 10}, 13, 1,
                   pal.onSurfaceVariant);
        DrawRectangle((int)70, (int)(124), (int)(GetScreenWidth() - 90),
                      (int)(32), pal.surfaceVariant);

        /* Status bar */
        {
            float sy = (float)GetScreenHeight() - 28;
            DrawRectangle(0, (int)sy, GetScreenWidth(), 28, pal.toolbarBg);
            DrawTextEx(app_font, status_text, (Vector2){12, sy + 7}, 13, 1,
                       pal.onSurfaceVariant);
        }

        EndDrawing();
    }

    tray_shutdown();
    bridge_free_processes(&procs);
    bridge_free_ports(&ports);
    CloseWindow();
    return 0;
}
