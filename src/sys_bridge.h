/* sys_bridge.h — Win32 系统操作桥接（打开/终端/URL/提权修复/单实例/自启动） */
#ifndef SYS_BRIDGE_H
#define SYS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

int SysShowInExplorer(const char *path);   /* 资源管理器选中文件 */
int SysOpenTerminal(const char *dir);      /* 在目录打开终端 */
int SysOpenUrl(const char *url);           /* 浏览器打开 */
int SysElevatedFix(int port);              /* UAC 提权 winnat 修复 0=已启动 1=取消 2=失败 */
void SysCopyFixCommand(int port, char *buf, int cap); /* 生成修复命令文本 */
int SysSingleInstance(void);               /* 0=唯一 1=已有实例（已唤起） */
int SysIsAutoRun(void);
int SysSetAutoRun(int on);                 /* 0 成功 */
void SysFlashTrayWindow(void);
int SysRelaunch(const char *path, const char *cmdline); /* 杀后原参数拉起 */
void SysClampWindowRect(int *x, int *y, int *w, int *h); /* 防呆：限制到虚拟屏幕内 */
int SysRestartElevated(void); /* 以管理员权限重启自身（成功不返回） */
void SysExeDir(char *out, int cap);          /* exe 所在目录（含尾反斜杠） */
int SysDownloadsDir(char *out, int cap);     /* 系统下载目录；0=成功 */
void SysTimestamp(char *out, int cap);       /* YYYY-MM-DD_HHmmss 本地时间戳 */
typedef struct {
    unsigned long pid;
    char title[128];      /* 主窗口标题（无窗口为空） */
    unsigned long threads;
    unsigned long handles;
    unsigned long privileges; /* 特权数（需权限，失败为 0） */
    char parentName[64];      /* 父进程名 */
    char startTime[32];       /* 进程启动时间 */
} SysProcDetail;

int SysQueryProcDetail(unsigned long pid, unsigned long ppid, SysProcDetail *out);

unsigned char *SysExtractIconRGBA(const char *exePath, int size, int *outW, int *outH); /* 调用方 free */             /* 唤起已有实例窗口 */

#ifdef __cplusplus
}
#endif

#endif /* SYS_BRIDGE_H */
