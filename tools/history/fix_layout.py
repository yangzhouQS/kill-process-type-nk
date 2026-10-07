# fix_layout.py — app_shared.c 剩余布局修正（行尾透明）
import io

P = r"src\app_shared.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

pairs = [
    # BeginList 行高
    ("float viewH = h - 30;", "float viewH = h - 44;"),
    ("float contentH = contentRows * 30.0f;", "float contentH = contentRows * (float)ROW_H;"),
    ("BeginScissorMode((int)x, (int)(y + 30), (int)w, (int)viewH);",
     "BeginScissorMode((int)x, (int)(y + 44), (int)w, (int)viewH);"),
    # 筛选栏
    ("float x = 8, y = 120;", "float x = 8, y = 152;"),
    ("DrawRectangleRounded((Rectangle){x, y, W, 36}, 0.20f, 8, gPal.cardBg);",
     "DrawRectangleRounded((Rectangle){x, y, W, 50}, 0.20f, 8, gPal.cardBg);"),
    # 状态栏
    ("float sy = (float)GetScreenHeight() - 26;", "float sy = (float)GetScreenHeight() - 38;"),
    ("DrawRectangle(0, (int)sy, (int)W, 26, gPal.toolbarBg);",
     "DrawRectangle(0, (int)sy, (int)W, 38, gPal.toolbarBg);"),
]

miss = []
for old, new in pairs:
    if old in t:
        t = t.replace(old, new)
    else:
        miss.append(old[:50])

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("done", "MISS:" if miss else "", miss)
