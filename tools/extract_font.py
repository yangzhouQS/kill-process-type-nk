from fontTools.ttLib import TTFont

src = r"assets\fonts\msyh.ttc"
f = TTFont(src, fontNumber=0, lazy=True)
print("family:", f["name"].getDebugName(1))
f.save(r"assets\fonts\msyh.ttf")
print("saved assets/fonts/msyh.ttf")
