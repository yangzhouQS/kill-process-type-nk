/* md_style.h — Material Design 3 主题系统（遵循 .kilo/rules.md）
 * 注意：调用者必须先 include raylib.h 和 nuklear.h（含 NK_IMPLEMENTATION）
 */
#ifndef MD_STYLE_H
#define MD_STYLE_H

#include "raylib.h"
/* nuklear 类型声明（NK_IMPLEMENTATION 由 main.c 定义，此处仅取声明） */
#include "nuklear.h"

/* ---- 深色主题色彩（MD3 dark scheme）---- */
#define MD_DARK_PRIMARY       ((struct nk_color){0xFF,0xFF,0xFF,0xFF}) /* 纯白主色 */
#define MD_DARK_ON_PRIMARY    ((struct nk_color){0x37,0x35,0x34,0xFF})
#define MD_DARK_PRIMARY_CONTAINER ((struct nk_color){0x4E,0x46,0x37,0xFF})
#define MD_DARK_ON_PRIMARY_CONTAINER ((struct nk_color){0xFF,0xFF,0xFF,0xFF})
#define MD_DARK_SURFACE       ((struct nk_color){0x14,0x12,0x11,0xFF})  /* 最深背景 */
#define MD_DARK_ON_SURFACE    ((struct nk_color){0xE5,0xE2,0xE0,0xFF})  /* 主文字 */
#define MD_DARK_SURFACE_VARIANT ((struct nk_color){0x49,0x45,0x44,0xFF}) /* 卡片背景 */
#define MD_DARK_ON_SURFACE_VARIANT ((struct nk_color){0xCA,0xC7,0xC5,0xFF})
#define MD_DARK_OUTLINE       ((struct nk_color){0x93,0x8F,0x8D,0xFF})
#define MD_DARK_ERROR         ((struct nk_color){0xFF,0xB4,0xAB,0xFF})
#define MD_DARK_SECONDARY_CONTAINER ((struct nk_color){0x33,0x30,0x2F,0xFF})

/* ---- 浅色主题色彩（MD3 light scheme）---- */
#define MD_LIGHT_PRIMARY      ((struct nk_color){0x1A,0x6B,0x3C,0xFF})  /* 绿色 */
#define MD_LIGHT_ON_PRIMARY   ((struct nk_color){0xFF,0xFF,0xFF,0xFF})
#define MD_LIGHT_SURFACE      ((struct nk_color){0xFD,0xF8,0xF3,0xFF})  /* 米白 */
#define MD_LIGHT_ON_SURFACE   ((struct nk_color){0x1C,0x1B,0x1A,0xFF})
#define MD_LIGHT_SURFACE_VARIANT ((struct nk_color){0xEF,0xEA,0xE4,0xFF})
#define MD_LIGHT_ON_SURFACE_VARIANT ((struct nk_color){0x49,0x45,0x44,0xFF})
#define MD_LIGHT_OUTLINE      ((struct nk_color){0x79,0x75,0x74,0xFF})
#define MD_LIGHT_ERROR        ((struct nk_color){0xB3,0x26,0x1E,0xFF})
#define MD_LIGHT_PRIMARY_CONTAINER ((struct nk_color){0xB9,0xF1,0xCA,0xFF})

/* ---- MD3 圆角 ---- */
#define MD_RADIUS_BUTTON      4.0f
#define MD_RADIUS_CARD        12.0f
#define MD_RADIUS_FAB         16.0f
#define MD_RADIUS_INPUT       4.0f
#define MD_RADIUS_CHIP        8.0f

/* ---- MD3 高程 ---- */
typedef enum { ELEV_0, ELEV_1, ELEV_2, ELEV_3 } Elevation;

/* ---- MD3 字号 ---- */
#define MD_FONT_TITLE         24
#define MD_FONT_BODY          16
#define MD_FONT_SMALL         14

/* ---- 8px 栅格 ---- */
#define MD_GRID               8
#define MD_PADDING_H          16  /* 按钮水平 padding */
#define MD_PADDING_V          8   /* 按钮垂直 padding */
#define MD_BTN_MIN_H          48  /* 按钮最小高度 */

/* 主题切换 */
typedef enum { MD_THEME_DARK = 0, MD_THEME_LIGHT = 1 } MdTheme;

/* 当前激活主题的全局指针（md_style_set 后只读） */
typedef struct {
    struct nk_color primary;
    struct nk_color on_primary;
    struct nk_color primary_container;
    struct nk_color surface;
    struct nk_color on_surface;
    struct nk_color surface_variant;
    struct nk_color on_surface_variant;
    struct nk_color outline;
    struct nk_color error;
    struct nk_color secondary_container;
} MdPalette;

extern MdPalette g_md;

/* 切换主题（更新 g_md 全局色板 + nuklear style） */
void md_style_set(struct nk_context *ctx, MdTheme theme);
MdTheme md_style_toggle(void);

#endif /* MD_STYLE_H */
