# fix_cells.py — 单元格截断 + 列宽拖拽基础设施
import io

P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

orig = t

# 1) Clip helper：插入在 DrawCellText 之前
helper = '''/* 文本按像素宽截断（超出加 …）；轮换静态缓冲 */
static const char *Clip(const char *s, float maxW)
{
    static char bufs[8][1024];
    static int idx = 0;
    char *b = bufs[idx];
    idx = (idx + 1) & 7;
    if (MeasureTxt(s, FS_TXT).x <= maxW)
        return s;
    snprintf(b, 1024, "%s", s);
    float target = maxW - MeasureTxt("…", FS_TXT).x - 2;
    int len = (int)strlen(b);
    while (len > 0 && MeasureTxt(b, FS_TXT).x > target) {
        /* 回退一个 UTF-8 字符 */
        len--;
        while (len > 0 && (b[len] & 0xC0) == 0x80) len--;
        b[len] = 0;
    }
    strcat(b, "…");
    return b;
}

'''
anchor = 'static void DrawCellText(const char *s, float x, float y, Color c)'
if 'static const char *Clip(' not in t:
    t = t.replace(anchor, helper + anchor)

# 2) 列宽数组去 const（可拖拽调整）
for name in ('gCwAll', 'gCwTree', 'gCwProj', 'gCwPort', 'gCwLog'):
    t = t.replace('static const float %s[]' % name, 'static float %s[]')

# 3) DrawTableHeader：列边界拖拽（参数 const float *cw -> float *cw）
t = t.replace(
    'void DrawTableHeader(float x, float y, float w, const char **cols,\n'
    '                     const float *cw, int ncols, int *sortCol,',
    'void DrawTableHeader(float x, float y, float w, const char **cols,\n'
    '                     float *cw, int ncols, int *sortCol,')

# 4) DrawTableHeader 函数体内加拖拽逻辑
old_hdr_body = '    DrawRectangle((int)x, (int)y, (int)w, 44, gPal.surfaceVariant);'
new_hdr_body = '''    DrawRectangle((int)x, (int)y, (int)w, 44, gPal.surfaceVariant);
    {
        /* 列宽拖拽：鼠标靠近列边界 ±5px 时按住左右拖动 */
        static int dragCol = -1;
        static float dragX = 0, dragW = 0;
        Vector2 m = GetMousePosition();
        if (dragCol >= 0) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float nw = dragW + (m.x - dragX);
                if (nw < 56) nw = 56;
                cw[dragCol] = nw;
            } else {
                dragCol = -1;
            }
        } else if (CheckCollisionPointRec(m, (Rectangle){x, y, w, 44})) {
            float edge = x + 8;
            for (int c = 0; c < ncols; c++) {
                edge += cw[c];
                if (c < ncols - 1 && m.x > edge - 5 && m.x < edge + 5) {
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        dragCol = c;
                        dragX = m.x;
                        dragW = cw[c];
                    }
                    break;
                }
            }
        }
    }'''
if old_hdr_body in t and '列宽拖拽' not in t:
    t = t.replace(old_hdr_body, new_hdr_body, 1)

# 5) 表头绘制处留 2px 拖拽边界余量（文字截断到列宽-14，避免压边界）
t = t.replace('DrawTxt(buf, cx, y + 9, FS_HDR, gPal.primary);',
              'DrawTxt(Clip(buf, cw[i] - 14), cx, y + 9, FS_HDR, gPal.primary);')
t = t.replace('DrawTxt(cols[i], cx, y + 9, FS_HDR, gPal.onSurfaceVariant);',
              'DrawTxt(Clip(cols[i], cw[i] - 14), cx, y + 9, FS_HDR, gPal.onSurfaceVariant);')

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("changed" if t != orig else "NO CHANGES")
