# patch15.py — CSV 导出确定性路径 + 自动定位 + 气泡
import io

# ============ sys_bridge: exe 目录 + 打开文件 ============
P = r"src\sys_bridge.h"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "SysExeDir" not in t:
    t = t.replace("int SysRestartElevated(void); /* 以管理员权限重启自身（成功不返回） */",
                  "int SysRestartElevated(void); /* 以管理员权限重启自身（成功不返回） */\nvoid SysExeDir(char *out, int cap);          /* exe 所在目录（含尾反斜杠） */")
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("sys.h ok")

P = r"src\sys_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
if "void SysExeDir" not in t:
    fn = '''
void SysExeDir(char *out, int cap)
{
    WCHAR exe[MAX_PATH];
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) {
        if (cap > 0) out[0] = 0;
        return;
    }
    WCHAR *slash = wcsrchr(exe, L'\\\\');
    if (slash) *slash = 0;
    WideCharToMultiByte(CP_UTF8, 0, exe, -1, out, cap, NULL, NULL);
}
'''
    t = t + "\n" + fn
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t)
    print("sys.c ok")

# ============ ui_views: ExportProcessesCsv 重写 ============
P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

old_start = "void ExportProcessesCsv(void)\n{\n    FILE *f = fopen(\"processes_export.csv\", \"wb\");"
i = t.find(old_start)
assert i >= 0, "csv func not found"
# 找函数结束（下一个行首 }）
j = t.find("\n}\n", i)
assert j > i, "csv func end not found"

new_fn = '''void ExportProcessesCsv(void)
{
    char dir[400];
    SysExeDir(dir, sizeof(dir));
    char path[600];
    snprintf(path, sizeof(path), "%s\\\\processes_export.csv", dir);

    FILE *f = fopen(path, "wb");
    if (!f) {
        SetFlashMsg("CSV 导出失败：%s", path);
        return;
    }
    /* UTF-8 BOM（Excel 兼容） */
    fwrite("\\xef\\xbb\\xbf", 1, 3, f);
    fprintf(f, "进程名,PID,父PID,内存MB,CPU%%,类型,AI风险,可执行路径,命令行,项目\\n");
    for (size_t i = 0; i < gApp.procs.count; i++) {
        BridgeProc *p = &gApp.procs.items[i];
        char pathEsc[600], cmdEsc[600], projEsc[300];
        snprintf(pathEsc, sizeof(pathEsc), "%s", p->path);
        snprintf(cmdEsc, sizeof(cmdEsc), "%s", p->cmdline);
        snprintf(projEsc, sizeof(projEsc), "%s", p->project);
        /* 内部 " 翻倍后整体包引号 */
        for (char *q = pathEsc; *q; q++)
            if (*q == '"') { memmove(q + 1, q, strlen(q) + 1); *q = '"'; q++; }
        for (char *q = cmdEsc; *q; q++)
            if (*q == '"') { memmove(q + 1, q, strlen(q) + 1); *q = '"'; q++; }
        for (char *q = projEsc; *q; q++)
            if (*q == '"') { memmove(q + 1, q, strlen(q) + 1); *q = '"'; q++; }
        fprintf(f, "\\"%s\\",%lu,%lu,%.1f,%.1f,\\"%s\\",%d,\\"%s\\",\\"%s\\",\\"%s\\"\\n",
                p->name, (unsigned long)p->pid, (unsigned long)p->ppid,
                (double)p->memBytes / 1048576.0,
                (double)(p->cpuPct < 0 ? 0 : p->cpuPct),
                ProcTypeName(p->type), p->aiRisk, pathEsc, cmdEsc, projEsc);
    }
    fclose(f);

    char msg[700];
    snprintf(msg, sizeof(msg), "已导出 %d 个进程到 %s",
             (int)gApp.procs.count, path);
    SetFlashMsg("%s", msg);
    if (gApp.balloonNotify)
        tray_notify("导出 CSV", msg);
    SysShowInExplorer(path);
}
'''
t = t[:i] + new_fn + t[j+3:]

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("csv export rewritten")
