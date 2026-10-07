/* main.c — kill-process-type-nk 主程序
 * 纯 raylib 2D UI + 中文界面 + 深/浅主题 + 托盘 + 单实例 + 自动刷新
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "raylib.h"
#include "app_shared.h"
#include "ui_views.h"
#include "ui_text.h"
#include "data_bridge.h"
#include "tray_bridge.h"
#include "ai_bridge.h"
#include "sys_bridge.h"
#include "version.h"

static int sRefreshPending = 0;

static void RefreshData(void)
{
    /* 异步：后台线程扫描，完成后由主循环 poll 交付（避免卡帧） */
    if (bridge_scan_async_start())
        sRefreshPending = 1;
    gApp.lastRefresh = GetTime();
}

static void RefreshPoll(void)
{
    if (!sRefreshPending)
        return;
    BridgeProcList procs;
    BridgePortList ports;
    if (bridge_scan_async_poll(&procs, &ports)) {
        bridge_free_processes(&gApp.procs);
        bridge_free_ports(&gApp.ports);
        gApp.procs = procs;
        gApp.ports = ports;
        sRefreshPending = 0;
        RebuildViews();
        bridge_monitor_sync(&gApp.procs);
    }
}

void MainUiRefresh(void)
{
    RefreshData();
    RebuildViews();
    /* 日志页签时同步刷新日志 */
    if (gApp.curTab == TAB_LOGS) {
        bridge_free_logs(&gApp.logs);
        bridge_load_logs(&gApp.logs);
    }
}

static void NotifyBalloon(const char *text)
{
    if (gApp.balloonNotify)
        tray_notify("Node/Python 进程终结者", text);
}

static void KillAllOfType(int type)
{
    const char *name = (type == 1) ? "Node" : "Python";
    int count = 0;
    for (size_t i = 0; i < gApp.procs.count; i++)
        if (gApp.procs.items[i].type == type)
            count++;
    if (count == 0) {
        SetFlashMsg("未发现 %s 进程", name);
        return;
    }
    int ok = 0, fail = 0;
    for (size_t i = 0; i < gApp.procs.count; i++) {
        if (gApp.procs.items[i].type == type) {
            if (bridge_kill_pid(gApp.procs.items[i].pid) == 0) ok++;
            else fail++;
        }
    }
    MainUiRefresh();
    char msg[128];
    snprintf(msg, sizeof(msg), "%s 清理完成：成功 %d，失败 %d", name, ok, fail);
    SetFlashMsg("%s", msg);
    NotifyBalloon(msg);
}

void MainToolbarAction(int id)
{
    switch (id) {
    case 0: MainUiRefresh(); break;
    case 1:
        if (gApp.selectedPid > 0) {
            bridge_kill_pid((unsigned int)gApp.selectedPid);
            MainUiRefresh();
            SetFlashMsg(N_KILLED_SEL);
            NotifyBalloon(N_KILLED_SEL);
        } else {
            SetFlashMsg(N_NO_SEL);
        }
        break;
    case 2: KillAllOfType(1); break;
    case 3: KillAllOfType(2); break;
    case 5: {
        int rc = SysRestartElevated();
        if (rc == 0) {
            NotifyBalloon(T_ADMIN_OK);
            exit(0); /* 管理员实例已启动，本实例退出 */
        }
        SetFlashMsg(T_ADMIN_FAIL);
        break;
    }
    case 4: {
        unsigned int *pids = NULL;
        int n = 0;
        CollectOrphanPids(&pids, &n);
        if (n == 0) {
            SetFlashMsg(N_ORPH_NONE);
            break;
        }
        int ok = 0;
        for (int i = 0; i < n; i++)
            if (bridge_kill_pid(pids[i]) == 0) ok++;
        free(pids);
        MainUiRefresh();
        char msg[128];
        snprintf(msg, sizeof(msg), N_KILLED_FMT, "手动", ok, n);
        SetFlashMsg("%s", msg);
        NotifyBalloon(msg);
        break;
    }
    }
}


/* ---------- 主程序 ---------- */

extern int CliRun(int argc, char **argv); /* cli.c */

int main(int argc, char **argv)
{
    if (argc >= 2 &&
        (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "/version") == 0)) {
        printf(APP_NAME " v" APP_VERSION "\n");
        return 0;
    }

    {
        int cr = CliRun(argc, argv);
        if (cr >= 0) return cr;
    }

    if (SysSingleInstance())
        return 0;

    bridge_init();

    int winX = (int)bridge_config_long("WinX", 80);
    int winY = (int)bridge_config_long("WinY", 60);
    int winW = (int)bridge_config_long("WinW", 1470);
    int winH = (int)bridge_config_long("WinH", 900);
    if (winW < 900) winW = 900;
    if (winH < 560) winH = 560;
    SysClampWindowRect(&winX, &winY, &winW, &winH);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(winW, winH, APP_TITLE);
    SetWindowPosition(winX, winY);
    SetTargetFPS(60);

    if (FileExists("assets/icon.png")) {
        Image icon = LoadImage("assets/icon.png");
        if (icon.data) {
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    }

    memset(&gApp, 0, sizeof(gApp));
    LoadAppFont();
    ApplyTheme((AppTheme)bridge_config_long("ui.theme", 0));

    gApp.autoRefreshOn = bridge_config_bool("AutoRefresh", 1);
    gApp.autoRefreshSec = bridge_config_long("AutoRefreshInterval", 10);
    gApp.orphanAutoEnable = bridge_config_bool("OrphanAutoEnable", 0);
    gApp.orphanIntervalMin = bridge_config_long("OrphanIntervalMin", 30);
    gApp.orphanNodePyOnly = bridge_config_bool("OrphanNodePyOnly", 1);
    gApp.anomalyWatch = bridge_config_bool("AnomalyWatch", 0);
    gApp.balloonNotify = bridge_config_bool("BalloonNotify", 1);
    gApp.anomalyMemMB = (int)bridge_config_long("AnomalyMemMB", 10);

    ViewsInit();
    ViewsLoadState();
    /* 恢复 AI 会话 */
    {
        FILE *f = fopen("ai_session.txt", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (n > 0 && n < 2 * 1024 * 1024) {
                gApp.aiOutputCap = (int)n + 64;
                gApp.aiOutput = (char *)malloc((size_t)gApp.aiOutputCap);
                size_t rd = fread(gApp.aiOutput, 1, (size_t)n, f);
                gApp.aiOutput[rd] = 0;
            }
            fclose(f);
        }
    }
    RefreshData();
    RebuildViews();
    bridge_free_logs(&gApp.logs);
    bridge_load_logs(&gApp.logs);

    tray_init();
    tray_hook_main_window();

    /* CPU/内存时序监控：启动采样线程，node/python 自动注册 */
    bridge_monitor_start();
    bridge_monitor_sync(&gApp.procs);

    double lastOrphanRun = GetTime();
    double lastAnomalyCheck = GetTime();
    if (bridge_config_bool("StartMinimized", 0))
        MinimizeToTray();

    while (!WindowShouldClose()) {
        int act = tray_poll();
        if (act == 1) {
            tray_toggle_main_window();
        }
        else if (act == 8) {
            int now = SysIsAutoRun();
            SysSetAutoRun(!now);
            gApp.autoRun = !now;
            if (gApp.balloonNotify)
                tray_notify("开机自启动",
                            !now ? "已开启：登录后自动驻留托盘。" : "已关闭。");
        }
        else if (act == 2) break;
        else if (act == 3) MainUiRefresh();
        else if (act == 4) KillAllOfType(1);
        else if (act == 5) KillAllOfType(2);
        else if (act == 6) MainToolbarAction(4);
        else if (act == 7) {
            gApp.modal = 1;
            SettingsLoad();
        }
        else if (act == 9) {
            /* AI 清理策略 */
            if (AiAvailable()) {
                gApp.modal = 2;
                gApp.aiMode = 5;
                if (gApp.aiOutput) gApp.aiOutput[0] = 0;
                AiStartCleanStrategy(&gApp.procs);
            } else {
                SetFlashMsg(A_NOKILO);
            }
        }
        else if (act == 10) BaselineSave();
        else if (act == 11) BaselineCompare();

        /* 键盘输入：AI 输入框 > 筛选框 */
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            if (gApp.modal == 2) {
                if ((ch >= 32 && ch < 127) || (ch >= 0x4E00 && ch <= 0x9FA5)) {
                    if (gApp.aiInputLen < 500) {
                        gApp.aiInput[gApp.aiInputLen++] = (char)ch;
                        gApp.aiInput[gApp.aiInputLen] = 0;
                    }
                }
            } else if (gApp.modal == 0) {
                if (ch >= 32 && ch < 127) {
                    if (gApp.filterLen < 250) {
                        gApp.filterBuf[gApp.filterLen++] = (char)ch;
                        gApp.filterBuf[gApp.filterLen] = 0;
                        RebuildViews();
                    }
                }
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (gApp.modal == 2 && gApp.aiInputLen > 0) {
                gApp.aiInput[--gApp.aiInputLen] = 0;
            } else if (gApp.modal == 0 && gApp.filterLen > 0) {
                gApp.filterBuf[--gApp.filterLen] = 0;
                RebuildViews();
            }
        }
        if (gApp.modal == 2 && IsKeyPressed(KEY_ENTER) && gApp.aiInputLen && !gApp.aiRunning)
            AiChatSubmit();
        if (IsKeyPressed(KEY_ESCAPE) && gApp.modal) {
            if (!gApp.menuOpen) gApp.modal = 0;
        }
        if (IsKeyPressed(KEY_F5))
            MainUiRefresh();

        /* 自动刷新 */
        if (gApp.autoRefreshOn && gApp.modal == 0 &&
            GetTime() - gApp.lastRefresh > (double)gApp.autoRefreshSec) {
            MainUiRefresh();
            bridge_monitor_sync(&gApp.procs);
        }

        /* 异常检测（AnomalyWatch）：node/python 内存 12s 连涨 >10MB 告警 */
        if (gApp.anomalyWatch && GetTime() - lastAnomalyCheck > 12.0) {
            lastAnomalyCheck = GetTime();
            for (size_t i = 0; i < gApp.procs.count; i++) {
                BridgeProc *p = &gApp.procs.items[i];
                unsigned long long growth = 0;
                if (p->type == 0) continue;
                if (bridge_monitor_mem_growth(p->pid, &growth) &&
                    growth >= (unsigned long long)gApp.anomalyMemMB) {
                    char msg[192];
                    snprintf(msg, sizeof(msg), T_ANOMALY_FMT, p->name,
                             (unsigned long)p->pid, (unsigned long)growth);
                    NotifyBalloon(msg);
                    break; /* 一次最多提示一个，避免气泡刷屏 */
                }
            }
        }

        /* 自动孤儿清理 */
        if (gApp.orphanAutoEnable &&
            GetTime() - lastOrphanRun > (double)gApp.orphanIntervalMin * 60.0) {
            lastOrphanRun = GetTime();
            unsigned int *pids = NULL;
            int n = 0;
            CollectOrphanPids(&pids, &n);
            if (n > 0) {
                int ok = 0;
                for (int i = 0; i < n; i++)
                    if (bridge_kill_pid(pids[i]) == 0) ok++;
                char msg[128];
                snprintf(msg, sizeof(msg), N_KILLED_FMT, "定时", ok, n);
                SetFlashMsg("%s", msg);
                NotifyBalloon(msg);
                MainUiRefresh();
            }
            free(pids);
        }

        RefreshPoll();
        DrawAiJobPoll();

        BeginDrawing();
        ClearBackground(gPal.surface);

        DrawToolbar();
        DrawTabBar();
        DrawFilterBar();

        float listY = 214;
        float listH = (float)GetScreenHeight() - listY - 36;
        DrawCurrentView(8, listY, (float)GetScreenWidth() - 16, listH);

        DrawStatusBar();
        if (gApp.modal == 0)
            DrawContextMenu();

        if (gApp.modal == 1)
            DrawSettingsModal();
        else if (gApp.modal == 2)
            DrawAiPanel();
        else if (gApp.modal == 3)
            DrawProcDetailModal();
        else if (gApp.modal == 4)
            DrawStatsModal();

        EndDrawing();
    }

    /* 保存会话状态 */
    ViewsSaveState();
    {
        if (gApp.aiOutput && gApp.aiOutput[0]) {
            FILE *f = fopen("ai_session.txt", "wb");
            if (f) {
                fwrite(gApp.aiOutput, 1, strlen(gApp.aiOutput), f);
                fclose(f);
            }
        } else {
            remove("ai_session.txt");
        }
    }

    /* 保存窗口位置 */
    Vector2 wp = GetWindowPosition();
    Vector2 ws = {(float)GetScreenWidth(), (float)GetScreenHeight()};
    bridge_config_set_long("WinX", (long)wp.x);
    bridge_config_set_long("WinY", (long)wp.y);
    bridge_config_set_long("WinW", (long)ws.x);
    bridge_config_set_long("WinH", (long)ws.y);

    tray_shutdown();
    AiShutdown();
    IconCacheFree();
    bridge_free_processes(&gApp.procs);
    bridge_free_ports(&gApp.ports);
    CloseWindow();
    return 0;
}
