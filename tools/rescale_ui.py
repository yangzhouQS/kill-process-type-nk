# rescale_ui.py — ui_views.c 字号/行高系统性缩放（ROW_H=36, FS_TXT=19, FS_HDR=18, FS_BTN=19）
import io
import re

P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

orig = t

# 0) 修复此前误伤：OrphanIntervalMin 默认值被换成 ROW_H
t = t.replace('bridge_config_long("OrphanIntervalMin", ROW_H)',
              'bridge_config_long("OrphanIntervalMin", 30)')

# 1) 行高：所有乘法与字面 30（行上下文）
t = t.replace("+ i * 30", "+ i * ROW_H")
t = t.replace("(float)i * 30", "(float)i * ROW_H")
t = t.replace("row * 30", "row * ROW_H")
t = t.replace("itemCount * 30", "itemCount * 36")
t = t.replace("y + h + 30", "y + h + ROW_H")
t = t.replace("{x, rowY, w, 30}", "{x, rowY, w, ROW_H}")
t = t.replace("(int)x, (int)rowY, (int)w, 30,", "(int)x, (int)rowY, (int)w, ROW_H,")
t = t.replace("(int)x, (int)rowY, (int)w, 30)", "(int)x, (int)rowY, (int)w, ROW_H)")
t = t.replace("(int)x, (int)rowY, (int)(GetScreenWidth() - 16), 30,",
              "(int)x, (int)rowY, (int)(GetScreenWidth() - 16), ROW_H,")
t = t.replace("Rectangle r = {x, *y, w, 30};", "Rectangle r = {x, *y, w, 36};")
t = t.replace("*y += 30;", "*y += 36;")

# 2) 行文本垂直偏移与字号：DrawCellText 主体 14 -> FS_TXT
t = t.replace("DrawTxt(s, x, y, 14, c);", "DrawTxt(s, x, y, FS_TXT, c);")

# 3) 列表内 DrawTxt 字号 14/15 -> FS_TXT/FS_HDR（仅 ui_views 内出现处）
t = t.replace('DrawTxt(head, x + 12, rowY + 7, 15, gPal.primary);',
              'DrawTxt(head, x + 12, rowY + 9, FS_BTN, gPal.primary);')

# 4) 菜单：宽 280->380，项字号 15->FS_BTN，边距
t = t.replace("float mw = 280;", "float mw = 380;")
t = t.replace("float mh = itemCount * 36 + 16;", "float mh = itemCount * 36 + 20;")
t = t.replace("float y = my + 8;", "float y = my + 10;")
t = t.replace("if (my + mh > H - 34) my = H - 34 - mh;",
              "if (my + mh > H - 40) my = H - 40 - mh;")
t = t.replace("DrawTxt(label, x + 12, *y + 7, 15, gPal.onSurface);",
              "DrawTxt(label, x + 12, *y + 8, FS_BTN, gPal.onSurface);")

# 5) 设置弹窗：面板与行距、控件字号
t = t.replace("float pw = 560, ph = 470;", "float pw = 700, ph = 580;")
t = t.replace("DrawTxt(S_TITLE, px + 20, py + 16, 20, gPal.onSurface);",
              "DrawTxt(S_TITLE, px + 20, py + 18, FS_TITLE, gPal.onSurface);")
t = t.replace("float y = py + 60;", "float y = py + 74;")
t = t.replace("(Rectangle){x, y, rowW, 28}, sSetStartMin)",
              "(Rectangle){x, y, rowW, 34}, sSetStartMin)")
t = t.replace("(Rectangle){x, y, rowW - 190, 28}, sSetAuto)",
              "(Rectangle){x, y, rowW - 230, 34}, sSetAuto)")
t = t.replace("(Rectangle){x, y, rowW, 28}, sSetBalloon)",
              "(Rectangle){x, y, rowW, 34}, sSetBalloon)")
t = t.replace("(Rectangle){x, y, rowW - 190, 28}, sSetOrphan)",
              "(Rectangle){x, y, rowW - 230, 34}, sSetOrphan)")
t = t.replace("(Rectangle){x, y, rowW, 28}, sSetOrphanNP)",
              "(Rectangle){x, y, rowW, 34}, sSetOrphanNP)")
t = t.replace("(Rectangle){x, y, rowW, 28}, sSetAnomaly)",
              "(Rectangle){x, y, rowW, 34}, sSetAnomaly)")
t = t.replace("(Rectangle){x, y, rowW, 28}, sSetAutoRun)",
              "(Rectangle){x, y, rowW, 34}, sSetAutoRun)")
t = t.replace("StepControl(S_INTERVAL_S, (Rectangle){x + rowW - 170, y, 170, 28}, sSetAutoSec, 3, 3600,",
              "StepControl(S_INTERVAL_S, (Rectangle){x + rowW - 210, y, 210, 34}, sSetAutoSec, 3, 3600,")
t = t.replace("StepControl(S_INTERVAL_M, (Rectangle){x + rowW - 170, y, 170, 28}, sSetOrphanMin, 5, 1440,",
              "StepControl(S_INTERVAL_M, (Rectangle){x + rowW - 210, y, 210, 34}, sSetOrphanMin, 5, 1440,")
t = t.replace("y += 36;", "y += 44;")
t = t.replace("y += 42;", "y += 50;")
t = t.replace("y += 44;\n\n    if (DrawTextButton(S_OPENLOGDIR",
              "y += 52;\n\n    if (DrawTextButton(S_OPENLOGDIR")
t = t.replace("DrawTxt(S_THEME, x, y + 4, 16, gPal.onSurface);",
              "DrawTxt(S_THEME, x, y + 5, FS_TXT, gPal.onSurface);")
t = t.replace("(Rectangle){x + 70, y, 100, 28}", "(Rectangle){x + 90, y, 110, 34}")
t = t.replace("(Rectangle){x + 180, y, 100, 28}", "(Rectangle){x + 210, y, 110, 34}")
t = t.replace("if (DrawTextButton(S_OPENLOGDIR, (Rectangle){x, y, 150, 32}, 1)) {",
              "if (DrawTextButton(S_OPENLOGDIR, (Rectangle){x, y, 190, 38}, 1)) {")
t = t.replace("if (DrawTextButton(S_CANCEL, (Rectangle){px + pw - 180, py + ph - 48, 80, 34}, 1))",
              "if (DrawTextButton(S_CANCEL, (Rectangle){px + pw - 224, py + ph - 58, 100, 40}, 1))")
t = t.replace("if (DrawTextButton(S_SAVE, (Rectangle){px + pw - 92, py + ph - 48, 72, 34}, 1)) {",
              "if (DrawTextButton(S_SAVE, (Rectangle){px + pw - 112, py + ph - 58, 92, 40}, 1)) {")

# 6) StepControl 字号与几何
t = t.replace("Vector2 ts = MeasureTxt(buf, 15);", "Vector2 ts = MeasureTxt(buf, FS_BTN);")
t = t.replace("Rectangle minus = {r.x, r.y, 28, 28};", "Rectangle minus = {r.x, r.y, 34, 34};")
t = t.replace("Rectangle plus = {r.x + r.width - 28, r.y, 28, 28};",
              "Rectangle plus = {r.x + r.width - 34, r.y, 34, 34};")
t = t.replace("Rectangle mid = {minus.x + 32, r.y + 2, r.width - 64, 24};",
              "Rectangle mid = {minus.x + 40, r.y + 3, r.width - 80, 28};")
t = t.replace('DrawTxt("-", minus.x + 10, minus.y + 4, 17, gPal.onPrimary);',
              'DrawTxt("-", minus.x + 12, minus.y + 4, FS_TITLE - 4, gPal.onPrimary);')
t = t.replace('DrawTxt("+", plus.x + 9, plus.y + 4, 17, gPal.onPrimary);',
              'DrawTxt("+", plus.x + 10, plus.y + 4, FS_TITLE - 4, gPal.onPrimary);')
t = t.replace("DrawTxt(label, r.x - MeasureTxt(label, 15).x - 12, r.y + 6, 15, gPal.onSurface);",
              "DrawTxt(label, r.x - MeasureTxt(label, FS_BTN).x - 12, r.y + 8, FS_BTN, gPal.onSurface);")
t = t.replace("DrawTxt(buf, mid.x + (mid.width - ts.x) / 2, mid.y + 4, 15, gPal.onSurface);",
              "DrawTxt(buf, mid.x + (mid.width - ts.x) / 2, mid.y + 6, FS_BTN, gPal.onSurface);")

# 7) 组头行高与行缓冲
t = t.replace("DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.surfaceVariant);",
              "DrawRectangle((int)x, (int)rowY, (int)w, ROW_H, gPal.surfaceVariant);")
t = t.replace("char tree[300];", "char tree[340];")

if t == orig:
    print("NO CHANGES")
else:
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("rescaled ok")
