/* data_bridge.h — 数据层桥接（隔离 Win32 与 raylib 的符号冲突）
 * main.c 只 include 此头文件，不直接 include process.h/net.h/config.h/klog.h
 */
#ifndef DATA_BRIDGE_H
#define DATA_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 精简进程信息（不含 Win32 类型） */
typedef struct {
    uint32_t pid;
    uint32_t ppid;
    char name[64];
    char path[260];
    char cmdline[208];
    char project[260];
    uint64_t memBytes;
    int type; /* 0=other 1=node 2=python */
    int aiRisk; /* 0=unknown 1=low 2=med 3=high */
    float cpuPct; /* CPU%%（monitor 采样，UI 填充） */
} BridgeProc;

typedef struct {
    BridgeProc *items;
    size_t count;
} BridgeProcList;

/* 精简端口信息 */
typedef struct {
    uint32_t port;
    uint32_t pid;
    int tcp;
    int ipv6;
} BridgePort;

typedef struct {
    BridgePort *items;
    size_t count;
} BridgePortList;

/* 精简日志信息 */
typedef struct {
    char timeText[24];
    char source[16];
    char name[64];
    uint32_t pid;
    int ok;
    char path[260];
} BridgeLog;

typedef struct {
    BridgeLog *items;
    size_t count;
} BridgeLogList;

/* 初始化（ConfigInit + KlogInit） */
void bridge_init(void);

/* 进程扫描（同步） */
void bridge_scan_processes(BridgeProcList *out);
void bridge_free_processes(BridgeProcList *l);

/* 进程扫描（异步：后台线程，主循环轮询交付） */
int bridge_scan_async_start(void);  /* 1=已启动（忙碌时返回 0） */
int bridge_scan_async_poll(BridgeProcList *procs, BridgePortList *ports); /* 1=完成并交付所有权 */

/* 端口扫描 */
void bridge_scan_ports(BridgePortList *out);
void bridge_free_ports(BridgePortList *l);

/* 日志加载 */
void bridge_load_logs(BridgeLogList *out);
void bridge_free_logs(BridgeLogList *l);

/* 保留端口区间（winnat excludedportrange） */
typedef struct {
    unsigned int start, end;
    int tcp;
} BridgeRange;

typedef struct {
    BridgeRange *items;
    size_t count;
} BridgeRangeList;

void bridge_scan_reserved(BridgeRangeList *out);
void bridge_free_reserved(BridgeRangeList *l);

/* 终止进程（自动落日志），返回 0=成功 */
int bridge_kill_pid(uint32_t pid);

/* 日志文件路径（宽字符，供打开文件夹用） */
const char *bridge_log_path_utf8(void);

/* CPU/内存时序监控（node/python 自动注册） */
void bridge_monitor_start(void);
void bridge_monitor_sync(const BridgeProcList *pl); /* 注册全部 node/python */
float bridge_monitor_cpu(uint32_t pid);            /* 最近一次 CPU%%（未监控返回 -1） */
int bridge_monitor_mem_growth(uint32_t pid, unsigned long long *growthMB); /* 12s 内存增量 */
int bridge_monitor_count(void);

/* 配置读写 */
int bridge_config_bool(const char *key, int def);
long bridge_config_long(const char *key, long def);
void bridge_config_set_bool(const char *key, int val);
void bridge_config_set_long(const char *key, long val);
void bridge_config_set_str(const char *key, const char *val);
const char *bridge_config_get_str(const char *key, const char *def);

#ifdef __cplusplus
}
#endif

#endif /* DATA_BRIDGE_H */
