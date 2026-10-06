# patch4.py — main.c: 管理员重启/clamp/monitor 接线（行尾透明）
import io

P = r"src\main.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

miss = []

def rep(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss.append(tag)

rep("{T_CLEAN_ORPHAN, 142, 4},", "{T_CLEAN_ORPHAN, 142, 4}, {T_ADMIN, 142, 5},", "btn")

rep("""    case 4: {
        unsigned int *pids = NULL;""",
    """    case 5: {
        int rc = SysRestartElevated();
        if (rc == 0) {
            NotifyBalloon(T_ADMIN_OK);
            exit(0); /* 管理员实例已启动，本实例退出 */
        }
        SetFlashMsg(T_ADMIN_FAIL);
        break;
    }
    case 4: {
        unsigned int *pids = NULL;""", "case5")

rep("""    if (winW < 900) winW = 900;
    if (winH < 560) winH = 560;""",
    """    if (winW < 900) winW = 900;
    if (winH < 560) winH = 560;
    SysClampWindowRect(&winX, &winY, &winW, &winH);""", "clamp")

rep("""    tray_init();
    tray_hook_main_window();""",
    """    tray_init();
    tray_hook_main_window();

    /* CPU/内存时序监控：启动采样线程，node/python 自动注册 */
    bridge_monitor_start();
    bridge_monitor_sync(&gApp.procs);""", "mon-start")

rep("""    double lastOrphanRun = GetTime();""",
    """    double lastOrphanRun = GetTime();
    double lastAnomalyCheck = GetTime();""", "anomaly-var")

rep("""        /* 自动刷新 */
        if (gApp.autoRefreshOn && gApp.modal == 0 &&
            GetTime() - gApp.lastRefresh > (double)gApp.autoRefreshSec)
            MainUiRefresh();""",
    """        /* 自动刷新 */
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
                if (bridge_monitor_mem_growth(p->pid, &growth)) {
                    char msg[192];
                    snprintf(msg, sizeof(msg), T_ANOMALY_FMT, p->name,
                             (unsigned long)p->pid, growth);
                    NotifyBalloon(msg);
                    break; /* 一次最多提示一个，避免气泡刷屏 */
                }
            }
        }""", "anomaly-check")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("MISS:", miss if miss else "none")
