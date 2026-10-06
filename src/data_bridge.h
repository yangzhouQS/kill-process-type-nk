/* data_bridge.h — 数据层桥接（隔离 Win32 与 raylib 的符号冲突）
 * main.c 只 include 此头文件，不直接 include process.h/net.h
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
} BridgeProc;

typedef struct {
    BridgeProc *items;
    size_t count;
} BridgeProcList;

/* 精简端口信息 */
typedef struct {
    uint32_t port;
    uint32_t pid;
    int tcp;  /* 1=TCP 0=UDP */
    int ipv6;
} BridgePort;

typedef struct {
    BridgePort *items;
    size_t count;
} BridgePortList;

/* 桥接 API */
void bridge_scan_processes(BridgeProcList *out);
void bridge_free_processes(BridgeProcList *l);
void bridge_scan_ports(BridgePortList *out);
void bridge_free_ports(BridgePortList *l);

#ifdef __cplusplus
}
#endif

#endif /* DATA_BRIDGE_H */
