# patch13.py — BeginList 输入门禁参数化 + 恢复底层列表绘制 + 详情标题去重
import io
import re

# ============ app_shared.h: BeginList 签名 ============
P = r"src\app_shared.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
t = t.replace("int  BeginList(float x, float y, float w, float h, float contentRows,\n                float *scroll); /* 裁剪+滚轮，返回内容起始 y */",
              "int  BeginList(float x, float y, float w, float h, float contentRows,\n                float *scroll, int inputEnabled); /* 裁剪+滚轮（inputEnabled=0 禁滚轮），返回内容起始 y */")
with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("hdr ok")

# ============ app_shared.c: BeginList 实现 + AI/详情/统计调用点 ============
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

rep("""int BeginList(float x, float y, float w, float h, float contentRows, float *scroll)
{
    float viewH = h - 44;
    float contentH = contentRows * (float)ROW_H;
    float maxOff = contentH > viewH ? contentH - viewH : 0.0f;
    Rectangle listR = {x, y, w, h};
    if (PointInRect(listR) && !UiInputBlocked()) {""",
"""int BeginList(float x, float y, float w, float h, float contentRows, float *scroll,
              int inputEnabled)
{
    float viewH = h - 44;
    float contentH = contentRows * (float)ROW_H;
    float maxOff = contentH > viewH ? contentH - viewH : 0.0f;
    Rectangle listR = {x, y, w, h};
    if (PointInRect(listR) && inputEnabled) {""", "beginlist-sig")

# AI 面板调用
rep("""    int baseY = BeginList(outR.x, outR.y, outR.width, outR.height, contentRows,
                          &gApp.scrollAiOut);""",
    """    int baseY = BeginList(outR.x, outR.y, outR.width, outR.height, contentRows,
                          &gApp.scrollAiOut, 1);""", "ai-call")

# 详情弹窗调用
rep("""    int baseY = BeginList(outR.x, outR.y, outR.width, outR.height, rows, &gApp.scrollAiOut);""",
    """    int baseY = BeginList(outR.x, outR.y, outR.width, outR.height, rows,
                          &gApp.scrollAiOut, 1);""", "detail-call")

# 详情文本去掉重复标题
rep('    snprintf(sDetailBuf, sizeof(sDetailBuf),\n             "进程详情\\n\\n"\n             "名称：',
    '    snprintf(sDetailBuf, sizeof(sDetailBuf),\n             "名称：', "detail-title")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("app_shared MISS:", miss if miss else "none")

# ============ ui_views.c: 底层调用点 ============
P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss2 = []

def rep2(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss2.append(tag)

# 五个底层视图的 BeginList 全部传 !UiInputBlocked()
n_before = t.count("BeginList(x, y, w, h,")
t = t.replace("BeginList(x, y, w, h, (float)sFilteredCount, &gApp.scrollProc)",
              "BeginList(x, y, w, h, (float)sFilteredCount, &gApp.scrollProc,\n                              !UiInputBlocked())")
t = t.replace("BeginList(x, y, w, h, (float)gApp.treeCount, &gApp.scrollTree)",
              "BeginList(x, y, w, h, (float)gApp.treeCount, &gApp.scrollTree,\n                              !UiInputBlocked())")
t = t.replace("BeginList(x, y, w, h, (float)gApp.projRowCount, &gApp.scrollProj)",
              "BeginList(x, y, w, h, (float)gApp.projRowCount, &gApp.scrollProj,\n                              !UiInputBlocked())")
t = t.replace("BeginList(x, y, w, h, (float)sPortRowCount, &gApp.scrollPort)",
              "BeginList(x, y, w, h, (float)sPortRowCount, &gApp.scrollPort,\n                              !UiInputBlocked())")
t = t.replace("BeginList(x, y, w, h, (float)gApp.logs.count, &gApp.scrollLog)",
              "BeginList(x, y, w, h, (float)gApp.logs.count, &gApp.scrollLog,\n                              !UiInputBlocked())")
print(f"ui_views calls updated: {n_before}")

# ============ main.c: 回滚 modal gate（恢复列表绘制） ============
P = r"src\main.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
old = """        float listY = 214;
        float listH = (float)GetScreenHeight() - listY - 36;
        if (gApp.modal == 0)
            DrawCurrentView(8, listY, (float)GetScreenWidth() - 16, listH);

        DrawStatusBar();
        if (gApp.modal == 0)
            DrawContextMenu();"""
new = """        float listY = 214;
        float listH = (float)GetScreenHeight() - listY - 36;
        DrawCurrentView(8, listY, (float)GetScreenWidth() - 16, listH);

        DrawStatusBar();
        if (gApp.modal == 0)
            DrawContextMenu();"""
if old in t:
    t = t.replace(old, new, 1)
    print("main gate rolled back")
else:
    print("main gate anchor MISS")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
