/* cli.c — CLI 子系统（/list /ports /kill /ai scan /--version）
 * GUI 子系统（-mwindows）下双击启动无控制台；命令行运行时
 * AttachConsole(ATTACH_PARENT_PROCESS) 重新接管父控制台输出。
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "data_bridge.h"
#include "ai_bridge.h"
#include "version.h"

/* 接管父控制台（GUI 子系统下双击启动时 stdout 无效才 attach+重定向；
 * 已有重定向（文件/管道）或已有控制台时保持原样，避免破坏输出 */
static void CliSetupConsole(void)
{
    DWORD ft = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
    if (ft == FILE_TYPE_UNKNOWN) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
}

/* 返回：<0 = 非 CLI（继续 GUI），>=0 = CLI 退出码 */
int CliRun(int argc, char **argv)
{
    if (argc < 2)
        return -1;

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "/version") == 0) {
        CliSetupConsole();
        printf(APP_NAME " v" APP_VERSION "\n");
        return 0;
    }

    if (strcmp(argv[1], "/list") == 0 || strcmp(argv[1], "/ports") == 0 ||
        strcmp(argv[1], "/kill") == 0 || strcmp(argv[1], "/ai") == 0) {
        CliSetupConsole();
    } else {
        return -1;
    }

    if (strcmp(argv[1], "/list") == 0) {
        bridge_init();
        BridgeProcList pl;
        bridge_scan_processes(&pl);
        printf("%-8s %-8s %-10s %-12s %s\n", "PID", "PPID", "MEM(MB)", "TYPE", "NAME");
        for (size_t i = 0; i < pl.count; i++) {
            printf("%-8lu %-8lu %-10.1f %-12s %s\n",
                   (unsigned long)pl.items[i].pid,
                   (unsigned long)pl.items[i].ppid,
                   (double)pl.items[i].memBytes / 1048576.0,
                   pl.items[i].type == 1 ? "node" :
                   pl.items[i].type == 2 ? "python" : "-",
                   pl.items[i].name);
        }
        bridge_free_processes(&pl);
        return 0;
    }

    if (strcmp(argv[1], "/ports") == 0) {
        bridge_init();
        BridgePortList nl;
        bridge_scan_ports(&nl);
        printf("%-8s %-6s %s\n", "PORT", "PROTO", "PID");
        for (size_t i = 0; i < nl.count; i++)
            printf("%-8lu %-6s %lu\n", (unsigned long)nl.items[i].port,
                   nl.items[i].tcp ? "TCP" : "UDP", (unsigned long)nl.items[i].pid);
        bridge_free_ports(&nl);
        BridgeRangeList rl;
        bridge_scan_reserved(&rl);
        printf("\nreserved ranges: %lu\n", (unsigned long)rl.count);
        for (size_t i = 0; i < rl.count; i++)
            printf("  %u-%u (%s)\n", rl.items[i].start, rl.items[i].end,
                   rl.items[i].tcp ? "TCP" : "UDP");
        bridge_free_reserved(&rl);
        return 0;
    }

    if (strcmp(argv[1], "/kill") == 0 && argc >= 3) {
        bridge_init();
        int ok = 0;
        for (int i = 2; i < argc; i++) {
            unsigned long pid = strtoul(argv[i], NULL, 10);
            if (bridge_kill_pid((unsigned int)pid) == 0) {
                printf("killed %lu\n", pid);
                ok++;
            } else {
                printf("failed %lu\n", pid);
            }
        }
        return ok == argc - 2 ? 0 : 1;
    }

    if (strcmp(argv[1], "/ai") == 0 && argc >= 3 && strcmp(argv[2], "scan") == 0) {
        bridge_init();
        BridgeProcList pl;
        bridge_scan_processes(&pl);
        AiStartRiskScan(&pl);
        int waited = 0;
        while (AiPoll() == 1 && waited < 300) {
            Sleep(1000);
            waited++;
        }
        if (AiPoll() == 2) {
            printf("%s\n", AiGetResult());
            AiConsumeResult();
        } else {
            fprintf(stderr, "ai scan failed: [%s] state=%d\n",
                    AiGetResult() ? AiGetResult() : "", AiPoll());
        }
        bridge_free_processes(&pl);
        return 0;
    }

    return -1;
}
