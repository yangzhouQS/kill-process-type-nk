# fix_trace_quotes.py — 修复 fprintf(stderr, '...跨行断开...'); 的单引号错误
import io
import re

for P in [r"src\main.c", r"src\ui_views.c"]:
    with io.open(P, encoding="utf-8") as f:
        t = f.read()
    n = 0
    while True:
        m = re.search(r"fprintf\(stderr, '([^']*?)\n'\);", t)
        if not m:
            break
        content = m.group(1)
        fixed = 'fprintf(stderr, "' + content + '\\n");'
        t = t[:m.start()] + fixed + t[m.end():]
        n += 1
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print(P, "fixed", n)
