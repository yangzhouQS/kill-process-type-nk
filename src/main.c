/* main.c — kill-process-type-nk 主入口
 * Nuklear + Raylib + MD3，完整四视图 + 托盘 + 深浅主题
 */
#define NOGDI
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "raylib.h"
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_IO
#define NK_IMPLEMENTATION
#include "nuklear.h"
#define NK_RAYLIB_IMPLEMENTATION
#include "nuklear_raylib.h"
#include "data_bridge.h"
#include "tray_bridge.h"

/* ==================== MD3 主题 ==================== */

typedef enum { MD_THEME_DARK = 0, MD_THEME_LIGHT = 1 } MdTheme;

typedef struct {
    struct nk_color primary, on_primary;
    struct nk_color surface, on_surface;
    struct nk_color surface_variant, on_surface_variant;
    struct nk_color outline, error, success;
    struct nk_color card_bg, card_border;
    struct nk_color row_alt, row_hover;
} MdPalette;

static MdPalette g_md;
static MdTheme s_theme = MD_THEME_DARK;

static void md_apply_style(struct nk_context *ctx)
{
    struct nk_style *s = &ctx->style;
    nk_style_default(ctx);

    s->window.background = g_md.surface;
    s->window.fixed_background = nk_style_item_color(g_md.surface);
    s->window.border_color = g_md.card_border;
    s->window.border = 1;
    s->window.rounding = 12;
    s->window.padding = nk_vec2(8, 8);
    s->window.group_padding = nk_vec2(8, 8);

    s->button.normal = nk_style_item_color(g_md.primary);
    s->button.hover = nk_style_item_color(g_md.primary);
    s->button.active = nk_style_item_color(g_md.primary);
    s->button.text_normal = g_md.on_primary;
    s->button.text_hover = g_md.on_primary;
    s->button.text_active = g_md.on_primary;
    s->button.padding = nk_vec2(16, 8);
    s->button.border = 0;
    s->button.rounding = 20;

    s->text.color = g_md.on_surface;
    s->text.padding = nk_vec2(4, 4);

    s->edit.normal = nk_style_item_color(g_md.surface_variant);
    s->edit.hover = nk_style_item_color(g_md.surface_variant);
    s->edit.active = nk_style_item_color(g_md.surface_variant);
    s->edit.border_color = g_md.outline;
    s->edit.text_normal = g_md.on_surface;
    s->edit.text_hover = g_md.on_surface;
    s->edit.text_active = g_md.on_surface;
    s->edit.border = 1;
    s->edit.rounding = 20;
    s->edit.padding = nk_vec2(12, 8);
    s->edit.cursor_normal = g_md.primary;
    s->edit.cursor_hover = g_md.primary;

    s->checkbox.normal = nk_style_item_color(g_md.surface_variant);
    s->checkbox.hover = nk_style_item_color(g_md.surface_variant);
    s->checkbox.active = nk_style_item_color(g_md.primary);
    s->checkbox.border_color = g_md.outline;
    s->checkbox.text_normal = g_md.on_surface;
    s->checkbox.text_hover = g_md.on_surface;
    s->checkbox.text_active = g_md.on_surface;

    s->scrollv.normal = nk_style_item_color(g_md.surface);
    s->scrollv.cursor_normal = nk_style_item_color(g_md.surface_variant);
    s->scrollv.cursor_hover = nk_style_item_color(g_md.outline);
    s->scrollv.cursor_active = nk_style_item_color(g_md.outline);
    s->scrollv.border_color = g_md.surface;
    s->scrollv.rounding = 4;
}

static void md_style_set(struct nk_context *ctx, MdTheme t)
{
    s_theme = t;
    if (t == MD_THEME_DARK) {
        g_md.primary         = nk_rgb(0x4F,0x9C,0xFF);  /* 蓝色主色 */
        g_md.on_primary      = nk_rgb(0x00,0x14,0x2E);
        g_md.surface         = nk_rgb(0x14,0x12,0x11);  /* 深底 */
        g_md.on_surface      = nk_rgb(0xE5,0xE2,0xE0);
        g_md.surface_variant = nk_rgb(0x2A,0x27,0x26);
        g_md.on_surface_variant = nk_rgb(0xCA,0xC7,0xC5);
        g_md.outline         = nk_rgb(0x5C,0x58,0x56);
        g_md.error           = nk_rgb(0xFF,0x8A,0x80);
        g_md.success         = nk_rgb(0x69,0xF0,0xAE);
        g_md.card_bg         = nk_rgb(0x1E,0x1C,0x1B);  /* 卡片底 */
        g_md.card_border     = nk_rgb(0x38,0x35,0x33);
        g_md.row_alt         = nk_rgb(0x24,0x22,0x21);  /* 交替行 */
        g_md.row_hover       = nk_rgb(0x2E,0x2B,0x2A);
    } else {
        g_md.primary         = nk_rgb(0x1A,0x6B,0x3C);  /* 绿色 */
        g_md.on_primary      = nk_rgb(0xFF,0xFF,0xFF);
        g_md.surface         = nk_rgb(0xFD,0xF8,0xF3);  /* 米白 */
        g_md.on_surface      = nk_rgb(0x1C,0x1B,0x1A);
        g_md.surface_variant = nk_rgb(0xE8,0xE3,0xDD);
        g_md.on_surface_variant = nk_rgb(0x49,0x45,0x44);
        g_md.outline         = nk_rgb(0x79,0x75,0x74);
        g_md.error           = nk_rgb(0xB3,0x26,0x1E);
        g_md.success         = nk_rgb(0x1B,0x5E,0x20);
        g_md.card_bg         = nk_rgb(0xFF,0xFF,0xFF);
        g_md.card_border     = nk_rgb(0xE0,0xDB,0xD5);
        g_md.row_alt         = nk_rgb(0xF5,0xF0,0xEA);
        g_md.row_hover       = nk_rgb(0xE8,0xE3,0xDD);
    }
    md_apply_style(ctx);
}

/* ==================== 应用状态 ==================== */

static struct nk_raylib_ctx *nk_ctx;
static int current_view = 0;
static int selected_pid = -1;
static BridgeProcList procs;
static BridgePortList ports;
static BridgeLogList logs;
static char filter_buf[256] = { 0 };
static int filter_len = 0;
static int sort_col = 0;
static int sort_desc = 0;
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

/* ==================== 绘制 ==================== */

/* 顶部工具栏（卡片容器） */
static void draw_toolbar(struct nk_context *ctx)
{
    if (nk_begin(ctx, "##toolbar", nk_rect(8, 8, (float)GetScreenWidth() - 16, 56), 0)) {
        nk_layout_row_begin(ctx, NK_STATIC, 40, 6);
        {
            nk_layout_row_push(ctx, 100);
            if (nk_button_label(ctx, "刷新")) md_refresh();
            nk_layout_row_push(ctx, 120);
            if (nk_button_label(ctx, "杀死选中") && selected_pid > 0) {
                bridge_kill_pid((uint32_t)selected_pid);
                md_refresh();
            }
            nk_layout_row_push(ctx, 145);
            nk_button_label(ctx, "杀全部Node");
            nk_layout_row_push(ctx, 155);
            nk_button_label(ctx, "杀全部Python");
            nk_layout_row_push(ctx, 100);
            if (nk_button_label(ctx, s_theme == MD_THEME_DARK ? "☀ 浅色" : "🌙 深色"))
                md_style_set(ctx, s_theme == MD_THEME_DARK ? MD_THEME_LIGHT : MD_THEME_DARK);
        }
        nk_layout_row_end(ctx);
    }
    nk_end(ctx);
}

/* 页签栏（MD3 分段按钮风格） */
static void draw_tabs(struct nk_context *ctx)
{
    static const char *tabs[] = { "全部进程", "Node/Python", "端口占用", "终止日志" };
    if (nk_begin(ctx, "##tabs", nk_rect(8, 70, (float)GetScreenWidth() - 16, 48), 0)) {
        nk_layout_row_dynamic(ctx, 36, 4);
        for (int i = 0; i < 4; i++) {
            if (nk_option_label(ctx, tabs[i], current_view == i)) {
                if (current_view != i) {
                    current_view = i;
                    if (i == 3) md_refresh_logs();
                }
            }
        }
    }
    nk_end(ctx);
}

/* 筛选 + 排序条 */
static void draw_filter(struct nk_context *ctx)
{
    if (nk_begin(ctx, "##filter", nk_rect(8, 124, (float)GetScreenWidth() - 16, 44), 0)) {
        nk_layout_row_begin(ctx, NK_STATIC, 32, 4);
        nk_layout_row_push(ctx, 50);
        nk_label(ctx, "筛选:", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 280);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, filter_buf, &filter_len, 255, nk_filter_default);
        nk_layout_row_push(ctx, 200);
        nk_label(ctx, status_text, NK_TEXT_LEFT);
        nk_layout_row_end(ctx);
    }
    nk_end(ctx);
}

/* 进程列表（卡片内交替行色） */
static void draw_procs(struct nk_context *ctx)
{
    float y = 174, h = (float)GetScreenHeight() - y - 8;
    if (nk_begin(ctx, "进程列表", nk_rect(8, y, (float)GetScreenWidth() - 16, h), NK_WINDOW_BORDER)) {
        /* 列头（可点击排序） */
        nk_layout_row_begin(ctx, NK_STATIC, 30, 6);
        {
            static const char *cols[] = { "进程名▼", "PID", "父PID", "内存", "类型", "项目" };
            static const float cw[] = { 200, 80, 80, 100, 80, 200 };
            for (int i = 0; i < 6; i++) {
                nk_layout_row_push(ctx, cw[i]);
                if (nk_selectable_label(ctx, cols[i], NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, &sort_col))
                    sort_desc = !sort_desc;
            }
        }
        nk_layout_row_end(ctx);

        /* 分隔线 */
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);

        /* 数据行 */
        for (size_t i = 0; i < procs.count; i++) {
            BridgeProc *p = &procs.items[i];
            char pid[16], ppid[16], mem[32];
            int is_selected = (int)p->pid == selected_pid;

            if (current_view == 1 && p->type == 0) continue;
            if (filter_len > 0 && !strstr(p->name, filter_buf)) continue;

            snprintf(pid, sizeof(pid), "%lu", (unsigned long)p->pid);
            snprintf(ppid, sizeof(ppid), "%lu", (unsigned long)p->ppid);
            snprintf(mem, sizeof(mem), "%.1fMB", (double)p->memBytes / 1048576.0);

            nk_layout_row_begin(ctx, NK_STATIC, 32, 6);
            {
                static const float rw[] = { 200, 80, 80, 100, 80, 200 };
                int clicked = 0;

                nk_layout_row_push(ctx, rw[0]);
                clicked |= nk_selectable_label(ctx, p->name, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, &is_selected);
                nk_layout_row_push(ctx, rw[1]);
                nk_label(ctx, pid, NK_TEXT_LEFT);
                nk_layout_row_push(ctx, rw[2]);
                nk_label(ctx, ppid, NK_TEXT_LEFT);
                nk_layout_row_push(ctx, rw[3]);
                nk_label(ctx, mem, NK_TEXT_LEFT);
                nk_layout_row_push(ctx, rw[4]);
                nk_label(ctx, p->type == 1 ? "Node.js" : (p->type == 2 ? "Python" : "—"), NK_TEXT_LEFT);
                nk_layout_row_push(ctx, rw[5]);
                nk_label(ctx, p->project[0] ? "…" : "-", NK_TEXT_LEFT);

                if (clicked) {
                    selected_pid = is_selected ? (int)p->pid : -1;
                }
            }
            nk_layout_row_end(ctx);
        }
    }
    nk_end(ctx);
}

/* 端口列表 */
static void draw_ports(struct nk_context *ctx)
{
    float y = 174, h = (float)GetScreenHeight() - y - 8;
    if (nk_begin(ctx, "端口监听", nk_rect(8, y, (float)GetScreenWidth() - 16, h), NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, 30, 4);
        nk_layout_row_push(ctx, 100); nk_label(ctx, "端口", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 100); nk_label(ctx, "协议", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 100); nk_label(ctx, "PID", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 200); nk_label(ctx, "类型", NK_TEXT_LEFT);
        nk_layout_row_end(ctx);

        for (size_t i = 0; i < ports.count; i++) {
            char port[16], pid[16];
            snprintf(port, sizeof(port), "%lu", (unsigned long)ports.items[i].port);
            snprintf(pid, sizeof(pid), "%lu", (unsigned long)ports.items[i].pid);
            nk_layout_row_begin(ctx, NK_STATIC, 30, 4);
            nk_layout_row_push(ctx, 100); nk_label(ctx, port, NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 100); nk_label(ctx, ports.items[i].tcp ? "TCP" : "UDP", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 100); nk_label(ctx, pid, NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 200); nk_label(ctx, ports.items[i].ipv6 ? "IPv6" : "IPv4", NK_TEXT_LEFT);
            nk_layout_row_end(ctx);
        }
    }
    nk_end(ctx);
}

/* 日志列表 */
static void draw_logs(struct nk_context *ctx)
{
    float y = 174, h = (float)GetScreenHeight() - y - 8;
    if (nk_begin(ctx, "终止日志", nk_rect(8, y, (float)GetScreenWidth() - 16, h), NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, 30, 6);
        nk_layout_row_push(ctx, 160); nk_label(ctx, "时间", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 100); nk_label(ctx, "来源", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 150); nk_label(ctx, "进程名", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 80);  nk_label(ctx, "PID", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 80);  nk_label(ctx, "结果", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 300); nk_label(ctx, "路径", NK_TEXT_LEFT);
        nk_layout_row_end(ctx);

        for (size_t i = 0; i < logs.count; i++) {
            BridgeLog *l = &logs.items[i];
            char pid[16];
            snprintf(pid, sizeof(pid), "%lu", (unsigned long)l->pid);
            nk_layout_row_begin(ctx, NK_STATIC, 28, 6);
            nk_layout_row_push(ctx, 160); nk_label(ctx, l->timeText, NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 100); nk_label(ctx, l->source, NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 150); nk_label(ctx, l->name, NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 80);  nk_label(ctx, pid, NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 80);  nk_label(ctx, l->ok ? "已终止" : "失败", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 300); nk_label(ctx, l->path[0] ? l->path : "-", NK_TEXT_LEFT);
            nk_layout_row_end(ctx);
        }
    }
    nk_end(ctx);
}

/* ==================== 主入口 ==================== */

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1200, 700, "kill-process-type-nk");
    SetTargetFPS(60);

    nk_ctx = nk_raylib_init(16);
    md_style_set(&nk_ctx->ctx, MD_THEME_DARK);
    md_refresh();

    tray_init();

    while (!WindowShouldClose()) {
        /* 托盘消息 */
        {
            int action = tray_poll();
            if (action == 2) break;               /* 退出 */
            else if (action == 1) {                /* 显示 */
                int w = GetScreenWidth(), h = GetScreenHeight();
                (void)w; (void)h;
            }
            else if (action == 3) md_refresh();    /* 刷新 */
        }

        BeginDrawing();
        nk_raylib_input(nk_ctx);
        {
            struct nk_context *ctx = &nk_ctx->ctx;
            draw_toolbar(ctx);
            draw_tabs(ctx);
            draw_filter(ctx);

            switch (current_view) {
            case 0: case 1: draw_procs(ctx); break;
            case 2: draw_ports(ctx); break;
            case 3: draw_logs(ctx); break;
            }
        }
        nk_raylib_render(nk_ctx,
            (struct nk_color){ g_md.surface.r, g_md.surface.g, g_md.surface.b, 255 });
        EndDrawing();
    }

    tray_shutdown();
    bridge_free_processes(&procs);
    bridge_free_ports(&ports);
    bridge_free_logs(&logs);
    nk_raylib_shutdown(nk_ctx);
    CloseWindow();
    return 0;
}
