# patch8.py — version 接线修复 + 内存复查 + AI上下文 + 详情/CSV/统计
import io

# ---------- main.c 修复 ----------
P = r"src\main.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss = []

def rep(old, new, tag, f=None):
    pass

def repm(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss.append("main:" + tag)

if '#include "version.h"' not in t:
    repm('#include "sys_bridge.h"', '#include "sys_bridge.h"\n#include "version.h"', "incl")
rep('InitWindow(winW, winH, "kill-process-type-nk");', 'InitWindow(winW, winH, APP_TITLE);', "title")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("main MISS:", miss if miss else "none")

# ---------- tray_bridge.c / sys_bridge.c: MAIN_WINDOW_TITLE 用 APP_TITLE ----------
for P2, anchor in [("src\\tray_bridge.c", 'lstrcpynW(s_nid.szTip, L"kill-process-type-nk", 128);'),
                   ("src\\sys_bridge.c", '#define MAIN_WINDOW_TITLE "kill-process-type-nk"')]:
    with io.open(P2, encoding="utf-8") as f:
        t2 = f.read()
    changed = False
    if '#include "version.h"' not in t2:
        t2 = t2.replace('#include "tray_bridge.h"', '#include "tray_bridge.h"\n#include "version.h"', 1)
        changed = True
    t2 = t2.replace('lstrcpynW(s_nid.szTip, L"kill-process-type-nk", 128);',
                    'lstrcpynW(s_nid.szTip, L"" APP_TITLE_W, 128);')
    t2 = t2.replace('#define MAIN_WINDOW_TITLE "kill-process-type-nk"',
                    '#define MAIN_WINDOW_TITLE APP_TITLE_A')
    # version.h 补 A/W 宏
    with io.open("src\\version.h", encoding="utf-8") as f:
        v = f.read()
    if "APP_TITLE_A" not in v:
        v = v.replace("#endif /* VERSION_H */",
"""#define APP_TITLE_A "kill-process-type-nk v" APP_VERSION   /* ANSI 用（FindWindow） */
#define APP_TITLE_W L"kill-process-type-nk v" APP_VERSION /* 宽字符用（托盘提示） */

#endif /* VERSION_H */""")
        with io.open("src\\version.h", "w", encoding="utf-8", newline="") as f:
            f.write(v)
    if changed or "APP_TITLE" in t2:
        with io.open(P2, "w", encoding="utf-8", newline="") as f:
            f.write(t2)
        print(P2, "ok")

# FindWindowA 的 ANSI 标题（sys_bridge/tray_bridge 两处 FindWindow 调用统一用 APP_TITLE_A）
for P2, old, new in [
    ("src\\tray_bridge.c", 'FindWindowA(NULL, "kill-process-type-nk")', 'FindWindowA(NULL, APP_TITLE_A)'),
    ("src\\sys_bridge.c", 'FindWindowA(NULL, MAIN_WINDOW_TITLE)', 'FindWindowA(NULL, APP_TITLE_A)'),
]:
    with io.open(P2, encoding="utf-8") as f:
        t2 = f.read()
    if old in t2:
        t2 = t2.replace(old, new)
        with io.open(P2, "w", encoding="utf-8", newline="") as f:
            f.write(t2)
        print(P2, "findwindow ok")

# version.h 的 A 宏需要和窗口标题一致：InitWindow 用 APP_TITLE（含 v7.0.0）
# FindWindow 匹配该标题 —— APP_TITLE_A 需 = APP_TITLE。修正 version.h：
with io.open("src\\version.h", encoding="utf-8") as f:
    v = f.read()
v = v.replace('#define APP_TITLE_A "kill-process-type-nk v" APP_VERSION   /* ANSI 用（FindWindow） */',
              '#define APP_TITLE_A APP_TITLE                              /* ANSI 用（FindWindow） */')
v = v.replace('#define APP_TITLE_W L"kill-process-type-nk v" APP_VERSION /* 宽字符用（托盘提示） */',
              '#define APP_TITLE_W L"" APP_TITLE                          /* 宽字符用（托盘提示） */')
with io.open("src\\version.h", "w", encoding="utf-8", newline="") as f:
    f.write(v)
print("version.h ok")
