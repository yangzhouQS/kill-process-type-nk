# cleanup_diag.py — 移除 RD/RV/HB/AR 诊断打点（保留 crash handler）
import io
import re

for P in [r"src\main.c", r"src\ui_views.c"]:
    with io.open(P, encoding="utf-8") as f:
        t = f.read()
    n0 = len(t)
    # TraceFile 行删除
    t = re.sub(r'\n *TraceFile\("[^"]*"\);', "", t)
    t = re.sub(r'\n *fflush\(stderr\);', "", t)
    # fprintf(stderr, ...) 诊断行删除（RD/RV/HB）
    t = re.sub(r'\n *fprintf\(stderr, "(?:RD|RV|HB)[^"]*"\);', "", t)
    # AR trigger 两行块
    t = t.replace('''            fprintf(stderr, "AR: auto refresh trigger");
            fflush(stderr);
''', "")
    # 心跳变量
    t = t.replace("\n    double lastHeartbeat = GetTime();", "")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print(P, "cleaned", n0 - len(t), "bytes")
