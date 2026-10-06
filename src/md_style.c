/* md_style.c — MD3 主题系统实现 */
#include "md_style.h"

MdPalette g_md;
static MdTheme s_current = MD_THEME_DARK;

static void md_apply_nk_style(struct nk_context *ctx, const MdPalette *p)
{
    struct nk_style *s = &ctx->style;

    /* 窗口背景 */
    nk_style_default(ctx);
    s->window.background = p->surface;
    s->window.fixed_background = nk_style_item_color(p->surface);
    s->window.border_color = p->surface_variant;
    s->window.padding = nk_vec2(MD_GRID, MD_GRID);
    s->window.group_padding = nk_vec2(MD_GRID, MD_GRID);

    /* 按钮：FilledButton 风格 */
    s->button.normal = nk_style_item_color(p->primary);
    s->button.hover = nk_style_item_color(p->primary);
    s->button.active = nk_style_item_color(p->primary);
    s->button.text_normal = p->on_primary;
    s->button.text_hover = p->on_primary;
    s->button.text_active = p->on_primary;
    s->button.padding = nk_vec2(MD_PADDING_H, MD_PADDING_V);
    s->button.border = 0;
    s->button.rounding = (int)MD_RADIUS_BUTTON;

    /* 文本 */
    s->text.color = p->on_surface;
    s->text.padding = nk_vec2(4, 4);

    /* 输入框 */
    s->edit.normal = nk_style_item_color(p->surface_variant);
    s->edit.hover = nk_style_item_color(p->surface_variant);
    s->edit.active = nk_style_item_color(p->surface_variant);
    s->edit.border_color = p->outline;
    s->edit.text_normal = p->on_surface;
    s->edit.text_hover = p->on_surface;
    s->edit.text_active = p->on_surface;
    s->edit.border = 1;
    s->edit.rounding = (int)MD_RADIUS_INPUT;
    s->edit.padding = nk_vec2(MD_GRID, MD_GRID);

    /* 勾选框 */
    s->checkbox.normal = nk_style_item_color(p->surface_variant);
    s->checkbox.hover = nk_style_item_color(p->surface_variant);
    s->checkbox.active = nk_style_item_color(p->primary);
    s->checkbox.border_color = p->outline;
    s->checkbox.text_normal = p->on_surface;
    s->checkbox.text_hover = p->on_surface;
    s->checkbox.text_active = p->on_surface;

    /* 滚动条 */
    s->scrollv.normal = nk_style_item_color(p->surface);
    s->scrollv.hover = nk_style_item_color(p->surface);
    s->scrollv.active = nk_style_item_color(p->surface);
    s->scrollv.cursor_normal = nk_style_item_color(p->surface_variant);
    s->scrollv.cursor_hover = nk_style_item_color(p->outline);
    s->scrollv.cursor_active = nk_style_item_color(p->outline);

    /* 属性/滑动条 */
    s->property.normal = nk_style_item_color(p->surface_variant);
    s->property.border_color = p->outline;
    s->property.rounding = (int)MD_RADIUS_INPUT;

    /* 组合框 */
    s->combo.normal = nk_style_item_color(p->surface_variant);
    s->combo.hover = nk_style_item_color(p->surface_variant);
    s->combo.active = nk_style_item_color(p->surface_variant);
    s->combo.border_color = p->outline;
    s->combo.text_normal = p->on_surface;
    s->combo.rounding = (int)MD_RADIUS_BUTTON;

    /* 选项卡 */
    s->tab.background = p->surface;
    s->tab.border_color = p->surface_variant;
    s->tab.text = p->on_surface;
}

void md_style_set(struct nk_context *ctx, MdTheme theme)
{
    s_current = theme;
    if (theme == MD_THEME_DARK) {
        g_md.primary = MD_DARK_PRIMARY;
        g_md.on_primary = MD_DARK_ON_PRIMARY;
        g_md.primary_container = MD_DARK_PRIMARY_CONTAINER;
        g_md.surface = MD_DARK_SURFACE;
        g_md.on_surface = MD_DARK_ON_SURFACE;
        g_md.surface_variant = MD_DARK_SURFACE_VARIANT;
        g_md.on_surface_variant = MD_DARK_ON_SURFACE_VARIANT;
        g_md.outline = MD_DARK_OUTLINE;
        g_md.error = MD_DARK_ERROR;
        g_md.secondary_container = MD_DARK_SECONDARY_CONTAINER;
    } else {
        g_md.primary = MD_LIGHT_PRIMARY;
        g_md.on_primary = MD_LIGHT_ON_PRIMARY;
        g_md.primary_container = MD_LIGHT_PRIMARY_CONTAINER;
        g_md.surface = MD_LIGHT_SURFACE;
        g_md.on_surface = MD_LIGHT_ON_SURFACE;
        g_md.surface_variant = MD_LIGHT_SURFACE_VARIANT;
        g_md.on_surface_variant = MD_LIGHT_ON_SURFACE_VARIANT;
        g_md.outline = MD_LIGHT_OUTLINE;
        g_md.error = MD_LIGHT_ERROR;
        g_md.secondary_container = MD_LIGHT_SURFACE_VARIANT;
    }
    md_apply_nk_style(ctx, &g_md);
}

MdTheme md_style_toggle(void)
{
    return s_current == MD_THEME_DARK ? MD_THEME_LIGHT : MD_THEME_DARK;
}
