/* tray_bridge.h — Win32 系统托盘桥接（与 raylib 主循环共存） */
#ifndef TRAY_BRIDGE_H
#define TRAY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化托盘（返回 0 成功） */
int tray_init(void);

/* 清理托盘 */
void tray_shutdown(void);

/* 处理待处理的托盘消息（每帧调用），返回动作代码：
 * 0=无, 1=显示窗口, 2=退出, 3=刷新, 4=杀全部Node, 5=杀全部Python, 6=清孤儿, 7=打开设置,
 * 8=切换开机自启动, 9=AI清理策略, 10=保存基线, 11=基线对比 */
int tray_poll(void);

/* 安装主窗口消息钩子：拦截点X -> 隐藏到托盘 */
void tray_hook_main_window(void);

/* 主窗口最小化到托盘（隐藏窗口） */
void MinimizeToTray(void);

/* 切换主窗口显示/隐藏（托盘左键），返回可见状态（1=现可见） */
int tray_toggle_main_window(void);

/* 托盘气泡通知 */
void tray_notify(const char *title, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* TRAY_BRIDGE_H */
