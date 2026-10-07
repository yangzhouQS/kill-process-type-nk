# patch16.py — CSV 导出到系统下载目录 + 时间戳命名
import io

# ============ sys_bridge: Downloads 目录 ============
P = r"src\sys_bridge.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "SysDownloadsDir" not in t:
    t = t.replace("void SysExeDir(char *out, int cap);          /* exe 所在目录（含尾反斜杠） */",
                  "void SysExeDir(char *out, int cap);          /* exe 所在目录（含尾反斜杠） */\nint SysDownloadsDir(char *out, int cap);     /* 系统下载目录；0=成功 */")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("sys.h ok")

P = r"src\sys_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "SysDownloadsDir" not in t:
    fn = '''
int SysDownloadsDir(char *out, int cap)
{
    PWSTR w = NULL;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &w)))
        return 1;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL);
    CoTaskMemFree(w);
    if (n <= 0 || n > cap) return 1;
    /* 去掉结尾反斜杠（若有） */
    if (n >= 2 && out[n - 2] == '\\\\')
        out[n - 2] = 0;
    return 0;
}
'''
    t = t + "\n" + fn
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("sys.c ok")

# ============ ui_views: ExportProcessesCsv 路径与命名 ============
P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

old_head = '''void ExportProcessesCsv(void)
{
    char dir[400];
    SysExeDir(dir, sizeof(dir));
    char path[600];
    snprintf(path, sizeof(path), "%s\\\\processes_export.csv", dir);

    FILE *f = fopen(path, "wb");'''
new_head = '''void ExportProcessesCsv(void)
{
    char dir[400];
    if (SysDownloadsDir(dir, sizeof(dir)) != 0)
        SysExeDir(dir, sizeof(dir)); /* 下载目录不可用时回退 exe 目录 */

    /* 时间戳：YYYY-MM-DD_HHmmss */
    SYSTEMTIME st;
    GetLocalTime(&st);
    char fname[64];
    snprintf(fname, sizeof(fname), "processes_%04d-%02d-%02d_%02d%02d%02d.csv",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    char path[600];
    snprintf(path, sizeof(path), "%s\\\\%s", dir, fname);

    FILE *f = fopen(path, "wb");'''
if old_head in t:
    t = t.replace(old_head, new_head, 1)
    print("path ok")
else:
    print("path MISS")

# 失败/成功提示更新
t = t.replace('''    if (!f) {
        SetFlashMsg("CSV 导出失败：%s", path);
        return;
    }''',
'''    if (!f) {
        SetFlashMsg("CSV 导出失败：%s", path);
        return;
    }''')

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("csv export updated")
