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
        d->cpuPct = 0.0f;
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

void bridge_scan_reserved(BridgeRangeList *out)
{
    PortRangeList rl;
    memset(&rl, 0, sizeof(rl));
    out->items = NULL;
    out->count = 0;
    if (ScanReservedPortRanges(&rl) > 0 && rl.count) {
        out->items = (BridgeRange *)malloc(rl.count * sizeof(BridgeRange));
        if (out->items) {
            for (size_t i = 0; i < rl.count; i++) {
                out->items[i].start = rl.items[i].start;
                out->items[i].end = rl.items[i].end;
                out->items[i].tcp = rl.items[i].tcp;
            }
            out->count = rl.count;
        }
    }
    FreePortRangeList(&rl);
}

void bridge_free_reserved(BridgeRangeList *l)
{
    free(l->items);
    l->items = NULL;
    l->count = 0;
}
void bridge_monitor_start(void)
{
    MonitorStart();
}

void bridge_monitor_sync(const BridgeProcList *pl)
{
    for (size_t i = 0; i < pl->count; i++) {
        if (pl->items[i].type != 0)
            MonitorAdd(pl->items[i].pid);
    }
}

float bridge_monitor_cpu(uint32_t pid)
{
    unsigned long long mem[60];
    double cpu[60];
    int n = MonitorGetSeries(pid, mem, cpu, 60);
    if (n <= 0)
        return -1.0f;
    return (float)cpu[n - 1];
}

int bridge_monitor_mem_growth(uint32_t pid, unsigned long long *growthMB)
{
    unsigned long long mem[60];
    double cpu[60];
    int n = MonitorGetSeries(pid, mem, cpu, 60);
    *growthMB = 0;
    if (n < 6)
        return 0;
    int increasing = 1;
    for (int j = n - 5; j < n; j++)
        if (mem[j] < mem[j - 1]) { increasing = 0; break; }
    unsigned long long growth = mem[n - 1] - mem[n - 6];
    if (increasing && growth > 10ULL * 1024 * 1024) {
        *growthMB = growth / (1024 * 1024);
        return 1;
    }
    return 0;
}

int bridge_monitor_count(void)
{
    return MonitorCount();
}

const char *bridge_log_path_utf8(void)
{
    static char buf[520];
    const WCHAR *w = KlogGetPath();
    if (!w) return ".";
    WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), NULL, NULL);
    return buf;
}
int bridge_kill_pid(uint32_t pid)
{
    KillResult kr;
    DWORD p = (DWORD)pid;
    KillPids(&p, 1, &kr, NULL);
    return kr.okCount > 0 ? 0 : -1;
}


/* ---------- 异步扫描（后台线程） ---------- */

#ifdef _WIN32
#include <windows.h>
typedef HANDLE bthread_t;
#else
#include <pthread.h>
typedef pthread_t bthread_t;
#endif

static volatile int sAsyncState = 0; /* 0 空闲 1 运行 2 完成 */
static BridgeProcList sAsyncProcs;
static BridgePortList sAsyncPorts;

#ifdef _WIN32
static DWORD WINAPI AsyncScanWorker(LPVOID arg)
#else
static void *AsyncScanWorker(void *arg)
#endif
{
    (void)arg;
    memset(&sAsyncProcs, 0, sizeof(sAsyncProcs));
    memset(&sAsyncPorts, 0, sizeof(sAsyncPorts));
    bridge_scan_processes(&sAsyncProcs);
    bridge_scan_ports(&sAsyncPorts);
    sAsyncState = 2;
    return 0;
}

int bridge_scan_async_start(void)
{
    if (sAsyncState == 1)
        return 0;
    if (sAsyncState == 2) {
        bridge_free_processes(&sAsyncProcs);
        bridge_free_ports(&sAsyncPorts);
        sAsyncState = 0;
    }
    sAsyncState = 1;
#ifdef _WIN32
    HANDLE th = CreateThread(NULL, 0, AsyncScanWorker, NULL, 0, NULL);
    if (!th) { sAsyncState = 0; return 0; }
    CloseHandle(th);
#else
    pthread_t th;
    if (pthread_create(&th, NULL, AsyncScanWorker, NULL) != 0) { sAsyncState = 0; return 0; }
    pthread_detach(th);
#endif
    return 1;
}

int bridge_scan_async_poll(BridgeProcList *procs, BridgePortList *ports)
{
    if (sAsyncState != 2)
        return 0;
    *procs = sAsyncProcs;
    *ports = sAsyncPorts;
    memset(&sAsyncProcs, 0, sizeof(sAsyncProcs));
    memset(&sAsyncPorts, 0, sizeof(sAsyncPorts));
    sAsyncState = 0;
    return 1;
}
