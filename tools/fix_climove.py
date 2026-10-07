# fix_climove.py — main.c: 删 RunCli 定义，换 CliRun 调用（行尾透明）
import io
import re

P = r"src\main.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

m = re.search(r'/\* ---------- CLI 模式.*?\nstatic int RunCli\(int argc, char \*\*argv\)\n\{.*?\n\}\n', t, re.S)
assert m, "RunCli block not found"
t = t[:m.start()] + t[m.end():]

old_call = """{
        int cr = RunCli(argc, argv);
        if (cr >= 0) return cr;
    }"""
assert old_call in t, "call not found"
t = t.replace(old_call, """{
        int cr = CliRun(argc, argv);
        if (cr >= 0) return cr;
    }""", 1)

t = t.replace("int main(int argc, char **argv)\n{",
              "extern int CliRun(int argc, char **argv); /* cli.c */\n\nint main(int argc, char **argv)\n{", 1)

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("main.c ok")
