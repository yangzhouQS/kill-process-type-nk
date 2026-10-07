/* ui_views.h — 视图模块接口 */
#ifndef UI_VIEWS_H
#define UI_VIEWS_H

#include "raylib.h"

void ViewsInit(void);
void RebuildViews(void);
void DrawCurrentView(float x, float y, float w, float h);
void OpenContextMenu(int kind, Vector2 pos, int pid, int portIdx);
void DrawContextMenu(void);
void DrawSettingsModal(void);
void SettingsLoad(void);
void SettingsSave(void);
void RefreshLogs(void);
void MainUiRefresh(void);      /* 刷新数据+重建视图+状态栏 */
void CollectOrphanPids(unsigned int **pids, int *count); /* 孤儿收集 */
void BaselineSave(void);
void BaselineCompare(void);
void ExportProcessesCsv(void); /* 全部进程导出 CSV */

#endif /* UI_VIEWS_H */
