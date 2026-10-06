/* data_bridge.c — 数据层桥接实现（在此文件中 include Win32 头文件） */
#include "data_bridge.h"
#include <stdlib.h>
#include <string.h>

/* 引入原版数据层（Win32 头文件在此隔离） */
#include "../../kill-process-type/src/process.h"
#include "../../kill-process-type/src/net.h"
#include "../../kill-process-type/src/klog.h"
#include "../../kill-process-type/src/config.h"

void bridge_init(void)
{
    ConfigInit();
    KlogInit();
}

int bridge_config_bool(const char *key, int def)
{
    WCHAR wkey[64];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 64);
    return ConfigGetBool(wkey, def);
}

long bridge_config_long(const char *key, long def)
{
    WCHAR wkey[64];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 64);
    return ConfigGetLong(wkey, def);
}

void bridge_config_set_bool(const char *key, int val)
{
    WCHAR wkey[64];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 64);
    ConfigSetBool(wkey, val);
}

void bridge_config_set_long(const char *key, long val)
{
    WCHAR wkey[64];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 64);
    ConfigSetLong(wkey, val);
}

void bridge_scan_processes(BridgeProcList *out)
{
    ProcList src;

    memset(out, 0, sizeof(*out));
    memset(&src, 0, sizeof(src));
    if (ScanAllProcesses(&src) < 0)
        return;
    out->items = (BridgeProc *)calloc(src.count ? src.count : 1, sizeof(BridgeProc));
    if (!out->items) {
        FreeProcList(&src);
        return;
    }
    for (size_t i = 0; i < src.count; i++) {
        BridgeProc *d = &out->items[i];
        ProcInfo *s = &src.items[i];
        d->pid = s->pid;
        d->ppid = s->ppid;
        d->memBytes = s->memBytes;
        d->type = (int)s->type;
        /* 宽字符转 UTF-8 */
        WideCharToMultiByte(CP_UTF8, 0, s->name, -1, d->name, 64, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, s->path, -1, d->path, 260, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, s->cmdline, -1, d->cmdline, 208, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, s->project, -1, d->project, 260, NULL, NULL);
    }
    out->count = src.count;
    FreeProcList(&src);
}

void bridge_free_processes(BridgeProcList *l)
{
    free(l->items);
    l->items = NULL;
    l->count = 0;
}

void bridge_scan_ports(BridgePortList *out)
{
    NetList src;

    memset(out, 0, sizeof(*out));
    memset(&src, 0, sizeof(src));
    if (ScanListenPorts(&src) < 0)
        return;
    out->items = (BridgePort *)calloc(src.count ? src.count : 1, sizeof(BridgePort));
    if (!out->items) {
        FreeNetList(&src);
        return;
    }
    for (size_t i = 0; i < src.count; i++) {
        out->items[i].port = src.items[i].port;
        out->items[i].pid = src.items[i].pid;
        out->items[i].tcp = src.items[i].tcp;
        out->items[i].ipv6 = src.items[i].ipv6;
    }
    out->count = src.count;
    FreeNetList(&src);
}

void bridge_free_ports(BridgePortList *l)
{
    free(l->items);
    l->items = NULL;
    l->count = 0;
}

void bridge_load_logs(BridgeLogList *out)
{
    memset(out, 0, sizeof(*out));
    ConfigInit();
    KlogInit();
    {
        LogList logs;
        memset(&logs, 0, sizeof(logs));
        KlogLoad(&logs);
        out->items = (BridgeLog *)calloc(logs.count ? logs.count : 1, sizeof(BridgeLog));
        if (!out->items) {
            KlogFree(&logs);
            return;
        }
        for (size_t i = 0; i < logs.count; i++) {
            BridgeLog *d = &out->items[i];
            LogEntry *s = &logs.items[i];
            WideCharToMultiByte(CP_UTF8, 0, s->timeText, -1, d->timeText, 24, NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, s->source, -1, d->source, 16, NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, s->name, -1, d->name, 64, NULL, NULL);
            d->pid = s->pid;
            d->ok = s->ok;
            WideCharToMultiByte(CP_UTF8, 0, s->path, -1, d->path, 260, NULL, NULL);
        }
        out->count = logs.count;
        KlogFree(&logs);
    }
}

void bridge_free_logs(BridgeLogList *l)
{
    free(l->items);
    l->items = NULL;
    l->count = 0;
}

int bridge_kill_pid(uint32_t pid)
{
    KillResult kr;
    DWORD p = (DWORD)pid;
    KillPids(&p, 1, &kr, NULL);
    return kr.okCount > 0 ? 0 : -1;
}
