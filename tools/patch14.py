# patch14.py — BeginListPlain（无表头列表）+ 弹窗滚动修复
import io

# ============ app_shared.h: BeginListPlain 声明 ============
P = r"src\app_shared.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "BeginListPlain" not in t:
    t = t.replace("int  BeginList(float x, float y, float w, float h, float contentRows,\n               float *scroll, int inputEnabled); /* 裁剪+滚轮（inputEnabled=0 禁滚轮），返回内容起始 y */",
"""int  BeginList(float x, float y, float w, float h, float contentRows,
               float *scroll, int inputEnabled); /* 裁剪+滚轮（有表头），返回内容起始 y */
int  BeginListPlain(float x, float y, float w, float h, float contentRows,
               float *scroll, int inputEnabled); /* 无表头版（弹窗内容区），返回内容起始 y */""")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("hdr ok")

# ============ app_shared.c: 实现 + 弹窗调用切换 ============
P = r"src\app_shared.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss = []

def rep(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss.append(tag)

# 1) 在 BeginList 实现后加 BeginListPlain（复用其结尾定位）
m = re.search(r'int BeginList\(float x, float y, float w, float h, float contentRows, float \*scroll,\n              int inputEnabled\)\n\{.*?\n\}\n', t, re.S)
assert m, "BeginList impl not found"
plain = r'''
int BeginListPlain(float x, float y, float w, float h, float contentRows,
                   float *scroll, int inputEnabled)
{
    float contentH = contentRows * (float)ROW_H;
    float maxOff = contentH > h ? contentH - h : 0.0f;
    Rectangle listR = {x, y, w, h};
    if (PointInRect(listR) && inputEnabled) {
        float mw = GetMouseWheelMove();
        if (mw != 0) {
            *scroll -= mw * 60.0f;
            if (*scroll < 0) *scroll = 0;
            if (*scroll > maxOff) *scroll = maxOff;
        }
    }
    BeginScissorMode((int)x, (int)y, (int)w, (int)h);
    return (int)(y - *scroll);
}
'''
t = t[:m.end()] + plain + t[m.end():]

# 2) AI 面板：BeginList -> BeginListPlain
rep("""    int baseY = BeginList(outR.x, outR.y, outR.width, outR.height, contentRows,
                          &gApp.scrollAiOut, 1);""",
    """    int baseY = BeginListPlain(outR.x, outR.y, outR.width, outR.height, contentRows,
                               &gApp.scrollAiOut, 1);""", "ai-plain")

# 3) 详情弹窗：BeginList -> BeginListPlain
rep("""    int baseY = BeginList(outR.x, outR.y, outR.width, outR.height, rows,
                          &gApp.scrollAiOut, 1);""",
    """    int baseY = BeginListPlain(outR.x, outR.y, outR.width, outR.height, rows,
                               &gApp.scrollAiOut, 1);""", "detail-plain")

# 4) 统计弹窗保持 BeginList（有表头）——检查它调用是否还在
if "int baseY = BeginList(x, y, w, h, (float)sStatCount" not in t and "sStatCount" in t:
    # 统计弹窗用行循环手绘（无 BeginList）✓ 无需改
    pass

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("app_shared MISS:", miss if miss else "none")
