# add_flush.py — stderr 打点全部加 fflush（崩溃前强制落盘）
import io
import re

for P in [r"src\main.c", r"src\ui_views.c"]:
    with io.open(P, encoding="utf-8") as f:
        t = f.read()
    n = 0
    def add_flush(m):
        global n
        n += 1
        return m.group(0) + "\n    fflush(stderr);"
    t = re.sub(r"fprintf\(stderr, \"[^\"]*\\\\n\"\);(?!\n    fflush)", add_flush, t)
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print(P, "flush added:", n)
