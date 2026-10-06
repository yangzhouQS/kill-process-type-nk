/* ai_bridge.h — kilo 无头 AI 异步调用（与 UI 解耦） */
#ifndef AI_BRIDGE_H
#define AI_BRIDGE_H

#include "data_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

int AiAvailable(void);                       /* kilo.exe 是否可找到 */
void AiStartChat(const char *msg);           /* 对话（带进程上下文） */
void AiStartAnalyze(const BridgeProc *p, const char *portsText); /* 单进程分析 */
int AiPoll(void);                            /* 0 空闲 1 运行 2 完成 3 失败 */
const char *AiGetResult(void);
void AiConsumeResult(void);
void AiShutdown(void);

/* ---- 扩展：批量与诊断类入口 ---- */
void AiStartRiskScan(const BridgeProcList *pl);      /* 批量风险 -> JSON */
void AiStartLogReview(const BridgeLogList *logs);    /* 日志复盘 */
void AiStartDiag(const BridgeProcList *pl);          /* 全局诊断快照 */
void AiStartCleanStrategy(const BridgeProcList *pl); /* AI 清理策略 */
int AiParseRiskJson(const char *text, unsigned int *pids, int *risks, int max);
int AiParseCleanJson(const char *text, unsigned int *pids, int max); /* 可终止清单 */

#ifdef __cplusplus
}
#endif

#endif /* AI_BRIDGE_H */
