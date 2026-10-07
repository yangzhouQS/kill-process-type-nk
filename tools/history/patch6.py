# patch6.py — 设置弹窗阈值控件 + FindPid/树构建哈希 + Markdown富文本 + 异步刷新 + dist + CLI /ai
import io

# ============ ui_views.c: 设置阈值 + FindPid 哈希 ============
P = r"src\ui_views.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()
miss = []

def rep(old, new, tag, cnt=1):
    global t
    if old in t:
        t = t.replace(old, new, cnt)
    else:
        miss.append(tag)

# 1) 设置变量/加载/保存：AnomalyMemMB
rep("static int sSetStartMin, sSetAuto, sSetAutoSec, sSetBalloon;\nstatic int sSetOrphan, sSetOrphanMin, sSetOrphanNP, sSetAnomaly, sSetAutoRun, sSetTheme;",
    "static int sSetStartMin, sSetAuto, sSetAutoSec, sSetBalloon;\nstatic int sSetOrphan, sSetOrphanMin, sSetOrphanNP, sSetAnomaly, sSetAnomalyMB;\nstatic int sSetAutoRun, sSetTheme;", "setvar")
rep('    sSetAnomaly = bridge_config_bool("AnomalyWatch", 0);',
    '    sSetAnomaly = bridge_config_bool("AnomalyWatch", 0);\n    sSetAnomalyMB = (int)bridge_config_long("AnomalyMemMB", 10);', "setload")
rep('    bridge_config_set_bool("AnomalyWatch", sSetAnomaly);',
    '    bridge_config_set_bool("AnomalyWatch", sSetAnomaly);\n    bridge_config_set_long("AnomalyMemMB", sSetAnomalyMB);\n    gApp.anomalyMemMB = sSetAnomalyMB;', "setsave")
rep("""    if (DrawCheckBox(S_ANOMALY, (Rectangle){x, y, rowW, 40}, sSetAnomaly))
        sSetAnomaly = !sSetAnomaly;""",
    """    if (DrawCheckBox(S_ANOMALY, (Rectangle){x, y, rowW - 260, 40}, sSetAnomaly))
        sSetAnomaly = !sSetAnomaly;
    StepControl("阈值MB", (Rectangle){x + rowW - 240, y, 240, 40}, sSetAnomalyMB, 5, 2000,
                &sSetAnomalyMB);""", "set-anomaly-ui")

# 2) FindPid 哈希（pid -> index，开放寻址）
rep("""static BridgeProc *FindPid(unsigned long pid)
{
    for (size_t i = 0; i < gApp.procs.count; i++)
        if (gApp.procs.items[i].pid == pid)
            return &gApp.procs.items[i];
    return NULL;
}""",
    """/* pid -> index 哈希（开放寻址），RebuildViews 时重建 */
#define PIDHASH_SIZE 2048
static int sPidHash[PIDHASH_SIZE];
static int sPidHashInit = 0;

static unsigned PidHashSlot(unsigned long pid)
{
    return (unsigned)((pid * 2654435761u) & (PIDHASH_SIZE - 1));
}

static void PidHashRebuild(void)
{
    for (int i = 0; i < PIDHASH_SIZE; i++)
        sPidHash[i] = -1;
    for (size_t i = 0; i < gApp.procs.count; i++) {
        unsigned h = PidHashSlot(gApp.procs.items[i].pid);
        while (sPidHash[h] >= 0)
            h = (h + 1) & (PIDHASH_SIZE - 1);
        sPidHash[h] = (int)i;
    }
    sPidHashInit = 1;
}

static BridgeProc *FindPid(unsigned long pid)
{
    if (!sPidHashInit)
        PidHashRebuild();
    unsigned h = PidHashSlot(pid);
    while (sPidHash[h] >= 0) {
        BridgeProc *p = &gApp.procs.items[sPidHash[h]];
        if (p->pid == pid)
            return p;
        h = (h + 1) & (PIDHASH_SIZE - 1);
    }
    return NULL;
}""", "pidhash")

# 3) RebuildViews 里重建哈希（过滤前）
rep("""void RebuildViews(void)
{
    /* 1) 过滤 */""",
    """void RebuildViews(void)
{
    PidHashRebuild();
    /* 1) 过滤 */""", "hash-rebuild")

# 4) 树构建 O(n^2) parent 查找 -> 用一次遍历构建 pid->index 表
rep("""    /* pid -> node 索引（线性查，进程几百个可接受） */
    int *parent = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) {
        BridgeProc *p = &sFiltered[i];
        parent[i] = -1;
        for (int j = 0; j < n; j++) {
            if (j != i && sFiltered[j].pid == p->ppid) {
                parent[i] = j;
                break;
            }
        }
    }""",
    """    /* pid -> index 哈希（复用 sPidHash 思路，局部表） */
    int *parent = (int *)malloc((size_t)n * sizeof(int));
    {
        static int idxMap[PIDHASH_SIZE];
        for (int i = 0; i < PIDHASH_SIZE; i++)
            idxMap[i] = -1;
        for (int i = 0; i < n; i++) {
            unsigned h = PidHashSlot(sFiltered[i].pid);
            while (idxMap[h] >= 0)
                h = (h + 1) & (PIDHASH_SIZE - 1);
            idxMap[h] = i;
        }
        for (int i = 0; i < n; i++) {
            parent[i] = -1;
            unsigned h = PidHashSlot(sFiltered[i].ppid);
            while (idxMap[h] >= 0) {
                int j = idxMap[h];
                if (j != i && sFiltered[j].pid == sFiltered[i].ppid) {
                    parent[i] = j;
                    break;
                }
                h = (h + 1) & (PIDHASH_SIZE - 1);
            }
        }
    }""", "tree-hash")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("ui_views MISS:", miss if miss else "none")
