/* app_shared.h — 全局状态/主题/控件/视图 声明（单向依赖：ui_views -> app_shared -> bridge） */
#ifndef APP_SHARED_H
#define APP_SHARED_H

#include "raylib.h"
#include "data_bridge.h"
#include "sys_bridge.h"

/* ---------- 主题 ---------- */

/* UI 尺寸常量（高 DPI 友好） */
#define ROW_H    44   /* 列表行高 */
#define FS_TXT   22   /* 正文/列表文本 */
#define FS_HDR   20   /* 表头/次要文本 */
#define FS_BTN   22   /* 按钮/页签/菜单 */
#define FS_TITLE 30   /* 弹窗标题 */

typedef enum { THEME_DARK = 0, THEME_LIGHT = 1 } AppTheme;

typedef struct {
    Color primary, onPrimary, primaryContainer;
    Color surface, onSurface, surfaceVariant, onSurfaceVariant;
    Color outline, errorC, successC, warnC;
    Color cardBg, cardBorder, rowAlt, rowHover, toolbarBg, selBg;
} Palette;

extern Palette gPal;

void ApplyTheme(AppTheme t);
AppTheme CurrentTheme(void);
void ToggleTheme(void);

/* ---------- 字体 ---------- */
extern Font gFont;
void LoadAppFont(void);
void DrawTxt(const char *text, float x, float y, float size, Color c);
const char *Clip(const char *s, float maxW); /* 超宽截断（…结尾） */
Texture2D IconForProc(const char *path);
void IconCacheFree(void);
Vector2 MeasureTxt(const char *text, float size);

/* ---------- 应用状态 ---------- */
enum { TAB_ALL = 0, TAB_NODEPY, TAB_TREE, TAB_PROJECT, TAB_PORTS, TAB_LOGS, TAB_AI, TAB_COUNT };

typedef struct {
    int curTab;
    BridgeProcList procs;
    BridgePortList ports;
    BridgeLogList logs;
    char filterBuf[256];
    int filterLen;
    int selectedPid;
    int selPorts[256];      /* 选中的端口行（保留区间行 pid=0xFFFFFFFF） */
    int selPortCount;
    char statusText[256];
    float scrollProc, scrollPort, scrollLog, scrollTree, scrollProj;
    float scrollAiOut;
    double lastRefresh;
    int autoRefreshOn;
    long autoRefreshSec;

    /* 树形/项目视图缓存 */
    BridgeProcList treeProcs;   /* 排序后的树序 */
    int *treeDepth;             /* 每行深度 */
    int treeCount;
    unsigned long long *treeMem; /* 子树内存合计 */
    int *treeHasKids;            /* 每行是否有子节点（▸▾） */
    BridgeProcList projProcs;   /* 项目分组排序 */
    int *projGroupStart;        /* 每组起始行 */
    char (*projGroupName)[260];
    int projGroupCount;
    int projRowCount;

    /* 主题/设置 */
    AppTheme theme;
    int startMinimized;
    int balloonNotify;
    int orphanAutoEnable;
    long orphanIntervalMin;
    int orphanNodePyOnly;
    int anomalyWatch;
    int autoRun;

    /* 弹窗 */
    int modal; /* 0=无 1=设置 2=AI */
    int aiMode; /* 0=对话 1=进程分析 */
    int aiPid;  /* 分析目标 */
    char aiInput[512];
    int aiInputLen;
    char *aiOutput;     /* AI 结果缓冲（动态） */
    int aiOutputCap;
    int aiRunning;
    int aiNeedResetScroll;

    /* 右键菜单 */
    int menuOpen;
    int menuKind; /* 0=进程 1=端口 2=保留区间 */
    Vector2 menuPos;
    int menuPid;
    int menuPortIdx;
    int menuRange[2];

    /* 端口保留区间 */
    void *reservedRanges;   /* RangeList* */
    int reservedCount;
} AppState;

extern AppState gApp;

/* ---------- 通用控件（app_shared.c） ---------- */
void DrawToolbar(void);
void DrawTabBar(void);
void DrawFilterBar(void);
void DrawStatusBar(void);
int  DrawTextButton(const char *label, Rectangle r, int enabled); /* 返回点击 */
int  DrawCheckBox(const char *label, Rectangle r, int checked);   /* 返回新状态 */
int  DrawRadio(const char *label, Rectangle r, int selected);
void DrawModalPanel(float w, float h); /* 居中遮罩+面板，返回内容区原点 */
void DrawTableHeader(float x, float y, float w, const char **cols,
                     const float *cw, int ncols, int *sortCol,
                     int *sortDesc);
int  BeginList(float x, float y, float w, float h, float contentRows,
               float *scroll); /* 裁剪+滚轮，返回内容起始 y */
void EndList(void);
void DrawAiPanel(void);
void AiChatSubmit(void);
void AiPanelShowText(const char *text); /* 清空输出并显示文本 */
void AiApplyRisk(void);                 /* 解析 AI 结果 JSON 并应用风险列 */
void AiApplyCleanStrategy(void);        /* 按清理策略 JSON 终止进程 */
void UiOnColumnResize(void);            /* 列宽拖拽结束回调（写配置） */
void DrawAiJobPoll(void);   /* 轮询异步 AI 结果 */

/* ---------- 视图（ui_views.c） ---------- */
void ViewsInit(void);
void RebuildViews(void);      /* 过滤+排序+树/项目重建 */
void DrawCurrentView(float x, float y, float w, float h);
void OpenContextMenu(int kind, Vector2 pos, int pid, int portIdx);
void DrawContextMenu(void);
void DrawSettingsModal(void);
void SettingsLoad(void);
void SettingsSave(void);

/* ---------- 内存格式化 ---------- */
void FormatMem(unsigned long long bytes, char *buf, int cap);
const char *ProcTypeName(int type);

/* 状态栏临时消息（8 秒后回落） */
void SetFlashMsg(const char *fmt, ...);

#endif /* APP_SHARED_H */
