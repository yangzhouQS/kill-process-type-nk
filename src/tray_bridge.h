/* tray_bridge.h — Win32 系统托盘桥接（与 raylib 主循环共存）
 * raylib 用 PollWindowEvents() 而非阻塞 WaitTime，托盘消息在 Win32 侧处理
 */
#ifndef TRAY_BRIDGE_H
#define TRAY_BRIDGE_H

#include <stdint.h>

/* 初始化托盘（返回 0 成功） */
int tray_init(void);

/* 清理托盘 */
void tray_shutdown(void);

/* 处理待处理的托盘消息（每帧调用），返回动作代码：
 * 0=无, 1=显示窗口, 2=退出, 3=刷新, 4=杀全部Node, 5=杀全部Python, 6=清孤儿 */
int tray_poll(void);

#endif
