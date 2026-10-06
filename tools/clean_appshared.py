# clean_appshared.py — 清理行首污染的文件头注释前缀
import io
import re

P = r"src\app_shared.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

# 保留真正的文件头（第一行），删除其余所有 "/* app_shared.c ... */" 行首前缀块
head = "/* app_shared.c — 主题/字体/全局状态/通用控件 实现 */"
# 统计污染次数
polluted = len(re.findall(re.escape(head), t))
t = re.sub(r"(/\* app_shared\.c[^\r\n]*?\*/)+", "", t)
# 恢复文件头（若被删）
if "通用控件 实现 */" not in t:
    t = head + "\n" + t
# 清理连续空行（3+ -> 1）
t = re.sub(r"\n{3,}", "\n\n", t)

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("polluted blocks removed:", polluted - 1)
