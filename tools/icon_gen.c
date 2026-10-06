/* icon_gen.c — 生成应用图标 assets/icon.png (256x256) */
#include "raylib.h"

int main(void)
{
    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "icon_gen");

    RenderTexture rt = LoadRenderTexture(256, 256);
    BeginTextureMode(rt);
    ClearBackground(BLANK);

    /* 深色圆角背景 */
    DrawRectangleRounded((Rectangle){8, 8, 240, 240}, 0.18f, 24,
                         (Color){0x1C, 0x22, 0x2E, 255});
    DrawRectangleRoundedLines((Rectangle){8, 8, 240, 240}, 0.18f, 24,
                              (Color){0x33, 0x3E, 0x50, 255});

    /* 准星圆环（蓝） */
    DrawRing((Vector2){128, 128}, 60, 74, 0, 360, 64, (Color){0x4F, 0x9C, 0xFF, 255});

    /* 四向刻度 */
    DrawRectangleRounded((Rectangle){122, 30, 12, 34}, 0.5f, 4, (Color){0x4F, 0x9C, 0xFF, 255});
    DrawRectangleRounded((Rectangle){122, 192, 12, 34}, 0.5f, 4, (Color){0x4F, 0x9C, 0xFF, 255});
    DrawRectangleRounded((Rectangle){30, 122, 34, 12}, 0.5f, 4, (Color){0x4F, 0x9C, 0xFF, 255});
    DrawRectangleRounded((Rectangle){192, 122, 34, 12}, 0.5f, 4, (Color){0x4F, 0x9C, 0xFF, 255});

    /* 中心终止符：红色圆 + 白色 X */
    DrawCircle(128, 128, 34, (Color){0xE0, 0x4F, 0x43, 255});
    DrawLineEx((Vector2){116, 116}, (Vector2){140, 140}, 9, WHITE);
    DrawLineEx((Vector2){140, 116}, (Vector2){116, 140}, 9, WHITE);

    /* 右下角 Node/Python 点缀：黄绿小圆 + 蓝小圆 */
    DrawCircle(196, 196, 14, (Color){0x69, 0xF0, 0xAE, 255});
    DrawCircle(196, 196, 7, (Color){0x1C, 0x22, 0x2E, 255});

    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    ExportImage(img, "assets/icon.png");
    UnloadImage(img);
    UnloadRenderTexture(rt);
    CloseWindow();
    return 0;
}
