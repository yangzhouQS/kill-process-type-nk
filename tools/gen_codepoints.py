# gen_codepoints.py — 扫描 ui_text.h + main 代码字符串，生成字体码点表
# 输出: src/font_codepoints.h (ASCII + GB2312 一级汉字 3755 + 文案补充字符)
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

chars = set()

# 1) ASCII 可打印区
for c in range(0x20, 0x7F):
    chars.add(chr(c))

# 2) 中文标点与常用符号
for s in "，。：；！？（）【】《》“”‘’、·—…%℃±×÷→←↑↓":
    chars.add(s)

# 3) GB2312 一级汉字（3755 个常用字，区位 16-55 区）
for hi in range(0xB0, 0xD8):
    for lo in range(0xA1, 0xFF):
        try:
            ch = bytes([hi, lo]).decode("gb2312")
            chars.add(ch)
        except UnicodeDecodeError:
            pass

# 4) 扫描项目源码字符串字面量中的其他字符（兜底）
import re
for name in os.listdir(os.path.join(ROOT, "src")):
    if not name.endswith((".h", ".c")):
        continue
    with io.open(os.path.join(ROOT, "src", name), encoding="utf-8", errors="ignore") as fp:
        text = fp.read()
    for m in re.finditer(r'"([^"\n]*)"', text):
        for ch in m.group(1):
            if ord(ch) > 0x7F:
                chars.add(ch)

cps = sorted(set(ord(c) for c in chars))
cps = [c for c in cps if c != 0x22 and c != 0x5C]  # 排除会破坏 C 字符串的引号
cps = [0x22, 0x5C] + cps  # 用转义放回

out = io.StringIO()
out.write("/* font_codepoints.h — 自动生成（tools/gen_codepoints.py），勿手改 */\n")
out.write("#ifndef FONT_CODEPOINTS_H\n#define FONT_CODEPOINTS_H\n\n")
out.write("#include <stddef.h>\n\n")
out.write("static const int kFontCodepoints[] = {\n")
line = "   "
for i, c in enumerate(cps):
    if c in (0x22, 0x5C):
        line += " 0x%X," % c
    else:
        line += " 0x%X," % c
    if (i + 1) % 8 == 0:
        out.write(line + "\n")
        line = "   "
if line.strip():
    out.write(line + "\n")
out.write("};\n\n")
out.write("#define FONT_CODEPOINT_COUNT %d\n\n" % len(cps))
out.write("#endif /* FONT_CODEPOINTS_H */\n")

dst = os.path.join(ROOT, "src", "font_codepoints.h")
with io.open(dst, "w", encoding="utf-8") as fp:
    fp.write(out.getvalue())
print("codepoints:", len(cps), "->", dst)
