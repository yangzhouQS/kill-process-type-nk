# patch2.py — app_shared.c: 页签缩小自适应/按钮居中/列宽持久化回调/CPU列辅助/AI策略执行
import io

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

# 1) 按钮文字垂直居中微调（+1px 校准字形基线）
rep("    DrawTxt(label, r.x + (r.width - ts.x) / 2, r.y + (r.height - ts.y) / 2,\n            FS_BTN,",
    "    DrawTxt(label, r.x + (r.width - ts.x) / 2, r.y + (r.height - ts.y) / 2 + 1,\n            FS_BTN,", "btn-center")

# 2) DrawTabBar 重写：高 40、自适应宽、居左
old_tab_start = "    float x = 8, y = 88;"
old_tab_end = "    }\n}\n\n/* ================= 筛选栏 ================= */"
i0 = t.find(old_tab_start)
i1 = t.find(old_tab_end)
if i0 < 0 or i1 < 0 or i1 <= i0:
    miss.append("tabbar-anchors")
else:
    new_tab = '''    float x = 8, y = 88;
    float W = (float)GetScreenWidth() - 16;

    DrawRectangle((int)x, (int)y, (int)W, 40, gPal.cardBg);
    {
        static const char *labels[TAB_COUNT] = {
            TT_TAB_ALL, TT_TAB_NODEPY, TT_TAB_TREE, TT_TAB_PROJECT,
            TT_TAB_PORTS, TT_TAB_LOGS, TT_TAB_AI
        };
        float cx = x;
        for (int i = 0; i < TAB_COUNT; i++) {
            float tw = MeasureTxt(labels[i], FS_BTN) + 48;
            Rectangle tr = {cx, y + 4, tw, 32};
            int sel = (gApp.curTab == i);
            if (PointInRect(tr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                gApp.curTab = i;
                if (i == TAB_LOGS) {
                    extern void RefreshLogs(void);
                    RefreshLogs();
                }
            }
            if (sel)
                DrawRectangleRec(tr, gPal.primary);
            else if (PointInRect(tr))
                DrawRectangleRec(tr, gPal.rowHover);
            Vector2 ts = MeasureTxt(labels[i], FS_BTN);
            DrawTxt(labels[i], tr.x + (tw - ts.x) / 2, tr.y + (32 - ts.y) / 2 + 1,
                    FS_BTN, sel ? gPal.onPrimary : gPal.onSurfaceVariant);
            cx += tw + 4;
        }
    }
}
'''
    t = t[:i0] + new_tab + t[i1 + len("    }\n}\n\n/* ================= 筛选栏 ================= */"):]
    # 重新接回筛选栏注释
    t = t.replace(new_tab + "\n", new_tab + "\n\n/* ================= 筛选栏 ================= */\n", 1) if "筛选栏" not in new_tab else t

# 3) 表头拖拽结束回调：dragCol 复位处调用 UiOnColumnResize
rep("            } else {\n                dragCol = -1;\n                SetMouseCursor(MOUSE_CURSOR_DEFAULT);\n            }",
    "            } else {\n                dragCol = -1;\n                SetMouseCursor(MOUSE_CURSOR_DEFAULT);\n                UiOnColumnResize();\n            }", "colresize-cb")
# 拖拽过程中也实时持久化（拖动每帧写太频繁——改为松开时写，已覆盖）

# 4) AI 面板：aiMode==5 策略按钮 + AiApplyCleanStrategy 实现
impl = '''void AiApplyCleanStrategy(void)
{
    if (!gApp.aiOutput)
        return;
    unsigned int pids[256];
    int n = AiParseCleanJson(gApp.aiOutput, pids, 256);
    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (bridge_kill_pid(pids[i]) == 0)
            ok++;
    }
    if (n > 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), A_APPLY_DONE, ok);
        SetFlashMsg("%s", msg);
        if (gApp.balloonNotify)
            tray_notify("AI 清理策略", msg);
        RebuildViews();
    }
}

void DrawAiJobPoll(void)'''
rep("void DrawAiJobPoll(void)", impl, "ai-clean-impl")

# 5) DrawAiPanel：aiMode==5 时显示“按策略终止”按钮（放在输出区与输入区之间）
rep("""    if (gApp.aiMode == 0) {
        Rectangle inR = {px + 16, py + ph - 74, pw - 32 - 96, 40};""",
    """    if (gApp.aiMode == 5 && !gApp.aiRunning && gApp.aiOutput && gApp.aiOutput[0]) {
        if (DrawTextButton(A_APPLY, (Rectangle){px + 16, py + ph - 70, 150, 40}, 1)) {
            AiApplyCleanStrategy();
            gApp.aiMode = 1;
        }
    }

    if (gApp.aiMode == 0) {
        Rectangle inR = {px + 16, py + ph - 74, pw - 32 - 96, 40};""",
    "ai-apply-btn")

# 6) 引入 ai_bridge（文件头已 include ✓ 检查）+ ui_views 的 UiOnColumnResize 是 extern 调用
if '#include "ui_views.h"' not in t:
    miss.append("ui_views-include")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("MISS:", miss if miss else "none")
