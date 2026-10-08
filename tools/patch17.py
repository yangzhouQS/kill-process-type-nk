# patch17.py — 打点转 crash-safe 文件日志 + 声明
import io
import re

# 1) app_shared.h: TraceLog 定义
P = r"src\app_shared.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "TraceLog" not in t:
    t += '''
/* 诊断追踪（每条 fopen append，崩溃安全） */
static void TraceLog(const char *msg)
{
    FILE *f = fopen("build/trace.log", "a");
    if (f) {
        fputs(msg, f);
        fputc('\\n', f);
        fclose(f);
    }
}
'''
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("hdr TraceLog ok")

# 2) main.c/ui_views.c: fprintf(stderr, "X\n"); fflush(stderr);  ->  TraceLog("X");
total = 0
for P in [r"src\main.c", r"src\ui_views.c"]:
    with io.open(P, encoding="utf-8") as f:
        t = f.read()
    pat = re.compile(r'fprintf\(stderr, "([^"]*)\\n"\);\r?\n    fflush\(stderr\);')
    t2, n = pat.subn(lambda m: 'TraceLog("' + m.group(1) + '");', t)
    total += n
    if n:
        with io.open(P, "w", encoding="utf-8", newline="") as f:
            f.write(t2)
    print(P, "converted", n)
print("total", total)
