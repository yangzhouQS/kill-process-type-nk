/* main.c — kill-process-type-nk 主入口（MD3 主题内联）
 * Nuklear + Raylib，通过 data_bridge 隔离 Win32
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

/* ==================== MD3 主题系统 ==================== */

typedef enum { MD_THEME_DARK = 0, MD_THEME_LIGHT = 1 } MdTheme;

typedef struct {
    struct nk_color primary;
    struct nk_color on_primary;
    struct nk_color surface;
    struct nk_color on_surface;
    struct nk_color surface_variant;
    struct nk_color on_surface_variant;
    struct nk_color outline;
    struct nk_color error;
} MdPalette;

static MdPalette g_md;
static MdTheme s_theme = MD_THEME_DARK;

static void md_apply_style(struct nk_context *ctx)
{
    struct nk_style *s = &ctx->style;
    nk_style_default(ctx);

    s->window.background = g_md.surface;
    s->window.fixed_background = nk_style_item_color(g_md.surface);
    s->window.border_color = g_md.surface_variant;
    s->window.padding = nk_vec2(8, 8);

    s->button.normal = nk_style_item_color(g_md.primary);
    s->button.hover = nk_style_item_color(g_md.primary);
    s->button.active = nk_style_item_color(g_md.primary);
    s->button.text_normal = g_md.on_primary;
    s->button.text_hover = g_md.on_primary;
    s->button.text_active = g_md.on_primary;
    s->button.padding = nk_vec2(16, 8);
    s->button.border = 0;
    s->button.rounding = 4;

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
    s->edit.rounding = 4;
    s->edit.padding = nk_vec2(8, 8);

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
}

static void md_style_set(struct nk_context *ctx, MdTheme t)
{
    s_theme = t;
    if (t == MD_THEME_DARK) {
        g_md.primary = nk_rgb(255,255,255);
        g_md.on_primary = nk_rgb(55,53,52);
        g_md.surface = nk_rgb(20,18,17);
        g_md.on_surface = nk_rgb(229,226,224);
        g_md.surface_variant = nk_rgb(73,69,68);
        g_md.on_surface_variant = nk_rgb(202,199,197);
        g_md.outline = nk_rgb(147,143,141);
        g_md.error = nk_rgb(255,180,171);
    } else {
        g_md.primary = nk_rgb(26,107,60);
        g_md.on_primary = nk_rgb(255,255,255);
        g_md.surface = nk_rgb(253,248,243);
        g_md.on_surface = nk_rgb(28,27,26);
        g_md.surface_variant = nk_rgb(239,234,228);
        g_md.on_surface_variant = nk_rgb(73,69,68);
        g_md.outline = nk_rgb(121,117,116);
        g_md.error = nk_rgb(179,38,30);
    }
    md_apply_style(ctx);
}

/* ==================== 应用状态 ==================== */

static struct nk_raylib_ctx *nk_ctx;
static int current_view = 0;
static BridgeProcList procs;
static BridgePortList ports;
static char filter_buf[256] = { 0 };
static int filter_len = 0;

static void md_refresh(void)
{
    bridge_free_processes(&procs);
    bridge_free_ports(&ports);
    bridge_scan_processes(&procs);
    bridge_scan_ports(&ports);
}

/* ==================== 视图绘制 ==================== */

static void draw_toolbar(struct nk_context *ctx)
{
    if (nk_begin(ctx, "##toolbar", nk_rect(0, 0, (float)GetScreenWidth(), 52), NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_begin(ctx, NK_STATIC, 40, 5);
        nk_layout_row_push(ctx, 110);
        if (nk_button_label(ctx, "刷新")) md_refresh();
        nk_layout_row_push(ctx, 130);
        nk_button_label(ctx, "杀死选中");
        nk_layout_row_push(ctx, 150);
        nk_button_label(ctx, "杀死全部Node");
        nk_layout_row_push(ctx, 160);
        nk_button_label(ctx, "杀死全部Python");
        nk_layout_row_push(ctx, 120);
        if (nk_button_label(ctx, "主题切换"))
            md_style_set(ctx, s_theme == MD_THEME_DARK ? MD_THEME_LIGHT : MD_THEME_DARK);
        nk_layout_row_end(ctx);
    }
    nk_end(ctx);
}

static void draw_tabs(struct nk_context *ctx)
{
    if (nk_begin(ctx, "##tabs", nk_rect(0, 56, (float)GetScreenWidth(), 48), NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 36, 4);
        if (nk_option_label(ctx, "全部进程", current_view == 0)) current_view = 0;
        if (nk_option_label(ctx, "Node/Python", current_view == 1)) current_view = 1;
        if (nk_option_label(ctx, "端口占用", current_view == 2)) current_view = 2;
        if (nk_option_label(ctx, "日志", current_view == 3)) current_view = 3;
    }
    nk_end(ctx);
}

static void draw_filter(struct nk_context *ctx)
{
    if (nk_begin(ctx, "##filter", nk_rect(0, 108, (float)GetScreenWidth(), 44), NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_begin(ctx, NK_STATIC, 32, 2);
        nk_layout_row_push(ctx, 50);
        nk_label(ctx, "筛选:", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 300);
        nk_edit_string(ctx, NK_EDIT_SIMPLE, filter_buf, &filter_len, 255, nk_filter_default);
        nk_layout_row_end(ctx);
    }
    nk_end(ctx);
}

static void draw_procs(struct nk_context *ctx)
{
    if (nk_begin(ctx, "进程", nk_rect(0, 156, (float)GetScreenWidth(), (float)GetScreenHeight() - 176), NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 28, 5);
        nk_label(ctx, "进程名", NK_TEXT_LEFT);
        nk_label(ctx, "PID", NK_TEXT_LEFT);
        nk_label(ctx, "内存", NK_TEXT_LEFT);
        nk_label(ctx, "类型", NK_TEXT_LEFT);
        nk_label(ctx, "项目", NK_TEXT_LEFT);

        for (size_t i = 0; i < procs.count; i++) {
            BridgeProc *p = &procs.items[i];
            char pid[16], mem[32];
            if (current_view == 1 && p->type == 0) continue;
            if (filter_len > 0 && !strstr(p->name, filter_buf)) continue;
            snprintf(pid, sizeof(pid), "%lu", (unsigned long)p->pid);
            snprintf(mem, sizeof(mem), "%.1fMB", (double)p->memBytes / 1048576.0);
            nk_layout_row_dynamic(ctx, 28, 5);
            nk_label(ctx, p->name, NK_TEXT_LEFT);
            nk_label(ctx, pid, NK_TEXT_LEFT);
            nk_label(ctx, mem, NK_TEXT_LEFT);
            nk_label(ctx, p->type == 1 ? "Node.js" : (p->type == 2 ? "Python" : "—"), NK_TEXT_LEFT);
            nk_label(ctx, p->project[0] ? "…" : "-", NK_TEXT_LEFT);
        }
    }
    nk_end(ctx);
}

static void draw_ports(struct nk_context *ctx)
{
    if (nk_begin(ctx, "端口", nk_rect(0, 156, (float)GetScreenWidth(), (float)GetScreenHeight() - 176), NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 28, 4);
        nk_label(ctx, "端口", NK_TEXT_LEFT);
        nk_label(ctx, "协议", NK_TEXT_LEFT);
        nk_label(ctx, "PID", NK_TEXT_LEFT);
        nk_label(ctx, "类型", NK_TEXT_LEFT);
        for (size_t i = 0; i < ports.count; i++) {
            char port[16], pid[16];
            snprintf(port, sizeof(port), "%lu", (unsigned long)ports.items[i].port);
            snprintf(pid, sizeof(pid), "%lu", (unsigned long)ports.items[i].pid);
            nk_layout_row_dynamic(ctx, 28, 4);
            nk_label(ctx, port, NK_TEXT_LEFT);
            nk_label(ctx, ports.items[i].tcp ? "TCP" : "UDP", NK_TEXT_LEFT);
            nk_label(ctx, pid, NK_TEXT_LEFT);
            nk_label(ctx, ports.items[i].ipv6 ? "IPv6" : "IPv4", NK_TEXT_LEFT);
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

    while (!WindowShouldClose()) {
        BeginDrawing();
        nk_raylib_input(nk_ctx);
        {
            struct nk_context *ctx = &nk_ctx->ctx;
            draw_toolbar(ctx);
            draw_tabs(ctx);
            draw_filter(ctx);
            if (current_view <= 1) draw_procs(ctx);
            else if (current_view == 2) draw_ports(ctx);
        }
        nk_raylib_render(nk_ctx,
            (struct nk_color){ g_md.surface.r, g_md.surface.g, g_md.surface.b, 255 });
        EndDrawing();
    }

    bridge_free_processes(&procs);
    bridge_free_ports(&ports);
    nk_raylib_shutdown(nk_ctx);
    CloseWindow();
    return 0;
}
