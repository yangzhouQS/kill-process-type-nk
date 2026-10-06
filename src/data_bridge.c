/* data_bridge.c — 数据层桥接实现（在此文件中 include Win32 头文件） */
#include "data_bridge.h"
#include <stdlib.h>
#include <string.h>

/* 引入原版数据层（Win32 头文件在此隔离） */
#include "../../kill-process-type/src/process.h"
#include "../../kill-process-type/src/net.h"

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
