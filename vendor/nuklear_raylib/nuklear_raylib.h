/* nuklear_raylib.h — Nuklear → Raylib 桥接层
 * 注意：调用者必须先 include raylib.h 和 nuklear.h（含 NK_IMPLEMENTATION）
 * 本文件不重复 include nuklear.h，避免函数重定义
 *
 * 正确用法：
 *   #include "raylib.h"
 *   #define NK_IMPLEMENTATION
 *   #include "nuklear.h"
 *   #define NK_RAYLIB_IMPLEMENTATION
 *   #include "nuklear_raylib.h"
 */
#ifndef NK_RAYLIB_H_
#define NK_RAYLIB_H_

/* nuklear.h 必须已由调用者包含 */

struct nk_raylib_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

struct nk_raylib_ctx {
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    struct nk_buffer cmds;
    struct nk_font *font;
    Texture2D font_tex;
    struct nk_draw_null_texture null_tex;
};

struct nk_raylib_ctx *nk_raylib_init(int font_size);
void nk_raylib_input(struct nk_raylib_ctx *ctx);
void nk_raylib_render(struct nk_raylib_ctx *ctx, struct nk_color bg);
void nk_raylib_shutdown(struct nk_raylib_ctx *ctx);

#endif /* NK_RAYLIB_H_ */

#ifdef NK_RAYLIB_IMPLEMENTATION
#ifndef NK_RAYLIB_IMPLEMENTATION_ONCE
#define NK_RAYLIB_IMPLEMENTATION_ONCE

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <rlgl.h>

struct nk_raylib_ctx *nk_raylib_init(int font_size)
{
    struct nk_raylib_ctx *ctx;

    ctx = (struct nk_raylib_ctx *)calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    nk_font_atlas_init_default(&ctx->atlas);
    nk_font_atlas_begin(&ctx->atlas);
    {
        int w, h;
        const void *img;

        if (font_size <= 0)
            font_size = 16;
        ctx->font = nk_font_atlas_add_default(&ctx->atlas, font_size, NULL);
        img = nk_font_atlas_bake(&ctx->atlas, &w, &h, NK_FONT_ATLAS_ALPHA8);
        {
            Image rimg = { 0 };
            rimg.data = (void *)img;
            rimg.width = w;
            rimg.height = h;
            rimg.mipmaps = 1;
            rimg.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
            ctx->font_tex = LoadTextureFromImage(rimg);
        }
        {
            nk_handle h;
            h.id = (int)ctx->font_tex.id;
            nk_font_atlas_end(&ctx->atlas, h, &ctx->null_tex);
        }
    }

    nk_init_default(&ctx->ctx, &ctx->font->handle);
    nk_buffer_init_default(&ctx->cmds);
    return ctx;
}

void nk_raylib_input(struct nk_raylib_ctx *ctx)
{
    struct nk_context *in = &ctx->ctx;
    nk_input_begin(in);

    {
        Vector2 pos = GetMousePosition();
        nk_input_motion(in, (int)pos.x, (int)pos.y);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            nk_input_button(in, NK_BUTTON_LEFT, (int)pos.x, (int)pos.y, 1);
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            nk_input_button(in, NK_BUTTON_LEFT, (int)pos.x, (int)pos.y, 0);
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            nk_input_button(in, NK_BUTTON_RIGHT, (int)pos.x, (int)pos.y, 1);
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
            nk_input_button(in, NK_BUTTON_RIGHT, (int)pos.x, (int)pos.y, 0);
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
            nk_input_button(in, NK_BUTTON_MIDDLE, (int)pos.x, (int)pos.y, 1);
        if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE))
            nk_input_button(in, NK_BUTTON_MIDDLE, (int)pos.x, (int)pos.y, 0);
    }
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
            nk_input_scroll(in, nk_vec2(0, wheel * 40.0f));
    }
    {
        int c = GetCharPressed();
        while (c > 0) {
            nk_input_unicode(in, (nk_rune)c);
            c = GetCharPressed();
        }
    }
    {
        if (IsKeyPressed(KEY_BACKSPACE)) nk_input_key(in, NK_KEY_BACKSPACE, 1);
        if (IsKeyReleased(KEY_BACKSPACE)) nk_input_key(in, NK_KEY_BACKSPACE, 0);
        if (IsKeyPressed(KEY_DELETE)) nk_input_key(in, NK_KEY_DEL, 1);
        if (IsKeyReleased(KEY_DELETE)) nk_input_key(in, NK_KEY_DEL, 0);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) nk_input_key(in, NK_KEY_ENTER, 1);
        if (IsKeyReleased(KEY_ENTER) || IsKeyReleased(KEY_KP_ENTER)) nk_input_key(in, NK_KEY_ENTER, 0);
        if (IsKeyPressed(KEY_TAB)) nk_input_key(in, NK_KEY_TAB, 1);
        if (IsKeyReleased(KEY_TAB)) nk_input_key(in, NK_KEY_TAB, 0);
        if (IsKeyPressed(KEY_LEFT)) nk_input_key(in, NK_KEY_LEFT, 1);
        if (IsKeyReleased(KEY_LEFT)) nk_input_key(in, NK_KEY_LEFT, 0);
        if (IsKeyPressed(KEY_RIGHT)) nk_input_key(in, NK_KEY_RIGHT, 1);
        if (IsKeyReleased(KEY_RIGHT)) nk_input_key(in, NK_KEY_RIGHT, 0);
        if (IsKeyPressed(KEY_UP)) nk_input_key(in, NK_KEY_UP, 1);
        if (IsKeyReleased(KEY_UP)) nk_input_key(in, NK_KEY_UP, 0);
        if (IsKeyPressed(KEY_DOWN)) nk_input_key(in, NK_KEY_DOWN, 1);
        if (IsKeyReleased(KEY_DOWN)) nk_input_key(in, NK_KEY_DOWN, 0);
        if (IsKeyPressed(KEY_HOME)) nk_input_key(in, NK_KEY_TEXT_LINE_START, 1);
        if (IsKeyReleased(KEY_HOME)) nk_input_key(in, NK_KEY_TEXT_LINE_START, 0);
        if (IsKeyPressed(KEY_END)) nk_input_key(in, NK_KEY_TEXT_LINE_END, 1);
        if (IsKeyReleased(KEY_END)) nk_input_key(in, NK_KEY_TEXT_LINE_END, 0);
        if (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) nk_input_key(in, NK_KEY_SHIFT, 1);
        if (IsKeyReleased(KEY_LEFT_SHIFT) || IsKeyReleased(KEY_RIGHT_SHIFT)) nk_input_key(in, NK_KEY_SHIFT, 0);
        if (IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)) nk_input_key(in, NK_KEY_CTRL, 1);
        if (IsKeyReleased(KEY_LEFT_CONTROL) || IsKeyReleased(KEY_RIGHT_CONTROL)) nk_input_key(in, NK_KEY_CTRL, 0);
    }

    nk_input_end(in);
}

void nk_raylib_render(struct nk_raylib_ctx *ctx, struct nk_color bg)
{
    struct nk_buffer vbuf, ebuf;
    struct nk_convert_config config;
    static const struct nk_draw_vertex_layout_element vertex_layout[] = {
        { NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(struct nk_raylib_vertex, position) },
        { NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(struct nk_raylib_vertex, uv) },
        { NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(struct nk_raylib_vertex, col) },
        { NK_VERTEX_LAYOUT_END }
    };
    const struct nk_draw_command *cmd;
    const nk_draw_index *offset = NULL;
    const struct nk_raylib_vertex *vertices;

    ClearBackground((Color){ bg.r, bg.g, bg.b, bg.a });

    memset(&config, 0, sizeof(config));
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(struct nk_raylib_vertex);
    config.vertex_alignment = 4;
    config.circle_segment_count = 22;
    config.curve_segment_count = 22;
    config.arc_segment_count = 22;
    config.global_alpha = 1.0f;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.line_AA = NK_ANTI_ALIASING_ON;
    config.tex_null = ctx->null_tex;

    nk_buffer_init_default(&vbuf);
    nk_buffer_init_default(&ebuf);
    nk_buffer_clear(&ctx->cmds);
    nk_convert(&ctx->ctx, &ctx->cmds, &vbuf, &ebuf, &config);

    vertices = (const struct nk_raylib_vertex *)nk_buffer_memory_const(&vbuf);
    offset = (const nk_draw_index *)nk_buffer_memory_const(&ebuf);

    nk_draw_foreach(cmd, &ctx->ctx, &ctx->cmds)
    {
        if (!cmd->elem_count)
            continue;

        BeginScissorMode(
            (int)cmd->clip_rect.x,
            (int)cmd->clip_rect.y,
            (int)cmd->clip_rect.w,
            (int)cmd->clip_rect.h
        );

        if (cmd->texture.id > 0)
            rlSetTexture((unsigned int)cmd->texture.id);
        else
            rlSetTexture(0);

        rlBegin(RL_TRIANGLES);
        {
            for (nk_draw_index i = 0; i < cmd->elem_count; i++) {
                const struct nk_raylib_vertex *v = &vertices[offset[i]];
                rlColor4ub(v->col[0], v->col[1], v->col[2], v->col[3]);
                rlTexCoord2f(v->uv[0], v->uv[1]);
                rlVertex2f(v->position[0], v->position[1]);
            }
        }
        rlEnd();

        EndScissorMode();
        offset += cmd->elem_count;
    }

    rlSetTexture(0);
    nk_clear(&ctx->ctx);
    nk_buffer_clear(&ctx->cmds);
    nk_buffer_free(&vbuf);
    nk_buffer_free(&ebuf);
}

void nk_raylib_shutdown(struct nk_raylib_ctx *ctx)
{
    if (!ctx)
        return;
    UnloadTexture(ctx->font_tex);
    nk_font_atlas_clear(&ctx->atlas);
    nk_buffer_free(&ctx->cmds);
    nk_free(&ctx->ctx);
    free(ctx);
}

#endif /* NK_RAYLIB_IMPLEMENTATION_ONCE */
#endif /* NK_RAYLIB_IMPLEMENTATION */
