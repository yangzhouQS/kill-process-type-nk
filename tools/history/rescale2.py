# rescale2.py — 二次放大：FS_TXT 19->22, FS_HDR 18->20, FS_BTN 19->22, FS_TITLE 26->30, ROW_H 36->44
import io

def patch(path, pairs):
    with io.open(path, encoding="utf-8") as f:
        t = f.read()
    miss = []
    for old, new in pairs:
        if old in t:
            t = t.replace(old, new)
        else:
            miss.append(old[:60])
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print(path, "ok" if not miss else "MISS: %d" % len(miss))
    for m in miss:
        print("  ", m)

patch(r"src\app_shared.h", [
    ('#define ROW_H    36', '#define ROW_H    44'),
    ('#define FS_TXT   19', '#define FS_TXT   22'),
    ('#define FS_HDR   18', '#define FS_HDR   20'),
    ('#define FS_BTN   19', '#define FS_BTN   22'),
    ('#define FS_TITLE 26', '#define FS_TITLE 30'),
])

patch(r"src\app_shared.c", [
    # 工具栏 64->72, 按钮 38->46, y 17->19
    ('(Rectangle){8, 8, W - 16, 64}, 0.10f, 8, gPal.toolbarBg);',
     '(Rectangle){8, 8, W - 16, 72}, 0.10f, 8, gPal.toolbarBg);'),
    ('float bx = 20, by = 17;', 'float bx = 20, by = 19;'),
    ('(Rectangle){bx, by, btns[i].w, 38}, 1))', '(Rectangle){bx, by, btns[i].w, 46}, 1))'),
    ('{T_REFRESH, 96, 0}, {T_KILL_SEL, 128, 1},',
     '{T_REFRESH, 108, 0}, {T_KILL_SEL, 142, 1},'),
    ('{T_KILL_NODE, 160, 2}, {T_KILL_PY, 172, 3},',
     '{T_KILL_NODE, 178, 2}, {T_KILL_PY, 190, 3},'),
    ('{T_CLEAN_ORPHAN, 128, 4},', '{T_CLEAN_ORPHAN, 142, 4},'),
    ('float tw = MeasureTxt(themeLabel, FS_BTN).x + 32;',
     'float tw = MeasureTxt(themeLabel, FS_BTN).x + 36;'),
    ('(Rectangle){rx, by, tw, 38}, 1))', '(Rectangle){rx, by, tw, 46}, 1))'),
    ('rx -= 96;\n    if (DrawTextButton(T_SETTINGS, (Rectangle){rx, by, 96, 38}, 1)) {',
     'rx -= 108;\n    if (DrawTextButton(T_SETTINGS, (Rectangle){rx, by, 108, 46}, 1)) {'),
    ('rx -= 128;\n    if (DrawTextButton(T_AI_CHAT, (Rectangle){rx, by, 128, 38}, 1)) {',
     'rx -= 142;\n    if (DrawTextButton(T_AI_CHAT, (Rectangle){rx, by, 142, 46}, 1)) {'),
    # 页签 80/48 -> 88/56
    ('float x = 8, y = 80;', 'float x = 8, y = 88;'),
    ('(int)W, 48, gPal.cardBg);', '(int)W, 56, gPal.cardBg);'),
    ('Rectangle tr = {x + i * tw, y, tw, 48};', 'Rectangle tr = {x + i * tw, y, tw, 56};'),
    ('tr.y + (48 - ts.y) / 2, FS_BTN,', 'tr.y + (56 - ts.y) / 2, FS_BTN,'),
    # 筛选 136/44 -> 152/50
    ('float x = 8, y = 136;', 'float x = 8, y = 152;'),
    ('(Rectangle){x, y, W, 44}, 0.20f, 8, gPal.cardBg);',
     '(Rectangle){x, y, W, 50}, 0.20f, 8, gPal.cardBg);'),
    ('DrawTxt("筛选", x + 12, y + 11, FS_BTN, gPal.onSurfaceVariant);',
     'DrawTxt("筛选", x + 12, y + 13, FS_BTN, gPal.onSurfaceVariant);'),
    ('Rectangle editR = {x + 76, y + 5, W - 90, 34};',
     'Rectangle editR = {x + 86, y + 6, W - 100, 38};'),
    # 状态栏 32->38
    ('float sy = (float)GetScreenHeight() - 32;', 'float sy = (float)GetScreenHeight() - 38;'),
    ('DrawRectangle(0, (int)sy, (int)W, 32, gPal.toolbarBg);',
     'DrawRectangle(0, (int)sy, (int)W, 38, gPal.toolbarBg);'),
    ('DrawTxt(sFlashBuf, 12, sy + 7, FS_HDR, gPal.warnC);',
     'DrawTxt(sFlashBuf, 12, sy + 9, FS_HDR, gPal.warnC);'),
    ('DrawTxt(gApp.statusText, 12, sy + 7, FS_HDR, gPal.onSurfaceVariant);',
     'DrawTxt(gApp.statusText, 12, sy + 9, FS_HDR, gPal.onSurfaceVariant);'),
    # 表头 36->44
    ('DrawRectangle((int)x, (int)y, (int)w, 36, gPal.surfaceVariant);',
     'DrawRectangle((int)x, (int)y, (int)w, 44, gPal.surfaceVariant);'),
    ('Rectangle hr = {cx - 8, y, cw[i], 36};', 'Rectangle hr = {cx - 8, y, cw[i], 44};'),
    ('DrawRectangle((int)x, (int)(y + 35), (int)w, 1, gPal.outline);',
     'DrawRectangle((int)x, (int)(y + 43), (int)w, 1, gPal.outline);'),
    # BeginList
    ('float viewH = h - 36;', 'float viewH = h - 44;'),
    ('BeginScissorMode((int)x, (int)(y + 36), (int)w, (int)viewH);',
     'BeginScissorMode((int)x, (int)(y + 44), (int)w, (int)viewH);'),
    # AI 面板按钮
    ('if (DrawTextButton(A_CLOSE, (Rectangle){px + pw - 90, py + 10, 70, 30}, 1)) {',
     'if (DrawTextButton(A_CLOSE, (Rectangle){px + pw - 104, py + 12, 84, 38}, 1)) {'),
    ('Rectangle inR = {px + 16, py + ph - 66, pw - 32 - 84, 34};',
     'Rectangle inR = {px + 16, py + ph - 74, pw - 32 - 96, 40};'),
    ('if (DrawTextButton(A_SEND, (Rectangle){inR.x + inR.width + 8, inR.y, 76, 34}, 1)',
     'if (DrawTextButton(A_SEND, (Rectangle){inR.x + inR.width + 8, inR.y, 88, 40}, 1)'),
    # AI 面板标题行距
    ('Rectangle outR = {px + 16, py + 52, pw - 32, ph - 130};',
     'Rectangle outR = {px + 16, py + 60, pw - 32, ph - 148};'),
    ('Rectangle inR = {px + 16, py + ph - 66, pw - 32 - 96, 40};',
     'Rectangle inR = {px + 16, py + ph - 74, pw - 32 - 96, 40};'),
])

patch(r"src\ui_views.c", [
    # 菜单 380->460, 项 36->44
    ('float mw = 380;', 'float mw = 460;'),
    ('Rectangle r = {x, *y, w, 36};', 'Rectangle r = {x, *y, w, 44};'),
    ('*y += 36;', '*y += 44;'),
    ('float y = my + 10;', 'float y = my + 12;'),
    ('DrawTxt(label, x + 12, *y + 8, FS_BTN, gPal.onSurface);',
     'DrawTxt(label, x + 12, *y + 10, FS_BTN, gPal.onSurface);'),
    ('float mh = itemCount * 36 + 20;', 'float mh = itemCount * 44 + 22;'),
    # 设置面板 700x580 -> 840x700
    ('float pw = 700, ph = 580;', 'float pw = 860, ph = 720;'),
    ('float y = py + 74;', 'float y = py + 88;'),
    ('y += 44;', 'y += 54;'),
    ('y += 50;', 'y += 60;'),
    ('y += 52;', 'y += 64;'),
    ('(Rectangle){x, y, rowW, 34}, sSetStartMin)', '(Rectangle){x, y, rowW, 40}, sSetStartMin)'),
    ('(Rectangle){x, y, rowW - 230, 34}, sSetAuto)', '(Rectangle){x, y, rowW - 260, 40}, sSetAuto)'),
    ('(Rectangle){x, y, rowW, 34}, sSetBalloon)', '(Rectangle){x, y, rowW, 40}, sSetBalloon)'),
    ('(Rectangle){x, y, rowW - 230, 34}, sSetOrphan)', '(Rectangle){x, y, rowW - 260, 40}, sSetOrphan)'),
    ('(Rectangle){x, y, rowW, 34}, sSetOrphanNP)', '(Rectangle){x, y, rowW, 40}, sSetOrphanNP)'),
    ('(Rectangle){x, y, rowW, 34}, sSetAnomaly)', '(Rectangle){x, y, rowW, 40}, sSetAnomaly)'),
    ('(Rectangle){x, y, rowW, 34}, sSetAutoRun)', '(Rectangle){x, y, rowW, 40}, sSetAutoRun)'),
    ('StepControl(S_INTERVAL_S, (Rectangle){x + rowW - 210, y, 210, 34}, sSetAutoSec, 3, 3600,',
     'StepControl(S_INTERVAL_S, (Rectangle){x + rowW - 240, y, 240, 40}, sSetAutoSec, 3, 3600,'),
    ('StepControl(S_INTERVAL_M, (Rectangle){x + rowW - 210, y, 210, 34}, sSetOrphanMin, 5, 1440,',
     'StepControl(S_INTERVAL_M, (Rectangle){x + rowW - 240, y, 240, 40}, sSetOrphanMin, 5, 1440,'),
    ('(Rectangle){x + 90, y, 110, 34}', '(Rectangle){x + 110, y, 130, 40}'),
    ('(Rectangle){x + 210, y, 110, 34}', '(Rectangle){x + 250, y, 130, 40}'),
    ('if (DrawTextButton(S_OPENLOGDIR, (Rectangle){x, y, 190, 38}, 1)) {',
     'if (DrawTextButton(S_OPENLOGDIR, (Rectangle){x, y, 220, 44}, 1)) {'),
    ('if (DrawTextButton(S_CANCEL, (Rectangle){px + pw - 224, py + ph - 58, 100, 40}, 1))',
     'if (DrawTextButton(S_CANCEL, (Rectangle){px + pw - 252, py + ph - 68, 116, 46}, 1))'),
    ('if (DrawTextButton(S_SAVE, (Rectangle){px + pw - 112, py + ph - 58, 92, 40}, 1)) {',
     'if (DrawTextButton(S_SAVE, (Rectangle){px + pw - 126, py + ph - 68, 106, 46}, 1)) {'),
    # StepControl 几何 34->40
    ('Rectangle minus = {r.x, r.y, 34, 34};', 'Rectangle minus = {r.x, r.y, 40, 40};'),
    ('Rectangle plus = {r.x + r.width - 34, r.y, 34, 34};',
     'Rectangle plus = {r.x + r.width - 40, r.y, 40, 40};'),
    ('Rectangle mid = {minus.x + 40, r.y + 3, r.width - 80, 28};',
     'Rectangle mid = {minus.x + 46, r.y + 4, r.width - 92, 32};'),
])

patch(r"src\main.c", [
    ('float listY = 190;', 'float listY = 214;'),
    ('int winW = (int)bridge_config_long("WinW", 1280);', 'int winW = (int)bridge_config_long("WinW", 1470);'),
    ('int winH = (int)bridge_config_long("WinH", 800);', 'int winH = (int)bridge_config_long("WinH", 900);'),
])
